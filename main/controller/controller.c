#include "controller.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt.h"
#include "schedule.h"
#include "sntp.h"
#include "zone.h"

#include <string.h>
#include <time.h>

/* Tunables ******************************************************************/

/* After this much idle time in MANUAL with no zone active, return to AUTO.
 * Change here if the spec evolves. */
#define MANUAL_AUTORETURN_SEC (30 * 60)

/* Default manual-run duration when no explicit duration is given (button
 * press). 0 means "run until the user stops it". */
#define MANUAL_DEFAULT_DURATION_SEC 0

#define TICK_MS 500

/* Events ********************************************************************/

typedef enum
{
    EV_BUTTON,
    EV_MANUAL_START,
    EV_MANUAL_STOP,
    EV_SET_MODE,
} event_id_t;

typedef struct
{
    event_id_t id;
    uint8_t zone;
    uint32_t duration_sec;
    controller_mode_t mode;
} event_t;

/* State *********************************************************************/

static const char* TAG = "controller";

static QueueHandle_t queue = NULL;

static controller_mode_t mode = MODE_AUTO;
static time_t run_until = 0; /* 0 = no auto-stop */
static time_t last_manual_event = 0;
static int16_t last_fired_minute = -1; /* hour*60+minute, dedup auto fires */

/* Helpers *******************************************************************/

static void publish_mode(void)
{
    mqtt_publish_mode(mode == MODE_AUTO ? "AUTO" : "MANUAL");
}

static void publish_zone_states(uint8_t prev_zone, uint8_t new_zone)
{
    if (prev_zone != new_zone)
    {
        if (prev_zone != ZONE_NONE) mqtt_publish_zone_state(prev_zone, false);
        if (new_zone != ZONE_NONE) mqtt_publish_zone_state(new_zone, true);
    }
}

static void switch_zone(uint8_t new_zone, time_t until)
{
    uint8_t prev = zone_get_active();
    zone_set_active(new_zone);
    run_until = until;
    publish_zone_states(prev, new_zone);
}

static void enter_mode(controller_mode_t new_mode)
{
    if (new_mode == mode) return;
    mode = new_mode;
    ESP_LOGI(TAG, "mode -> %s", mode == MODE_AUTO ? "AUTO" : "MANUAL");
    publish_mode();
}

static void touch_manual_activity(void) { last_manual_event = time(NULL); }

/* Event handlers ************************************************************/

static void handle_button(uint8_t zone)
{
    if (zone >= ZONE_COUNT) return;

    enter_mode(MODE_MANUAL);
    touch_manual_activity();

    uint8_t active = zone_get_active();
    if (active == zone)
    {
        switch_zone(ZONE_NONE, 0);
    }
    else
    {
        time_t until = 0;
        if (MANUAL_DEFAULT_DURATION_SEC > 0)
        {
            until = time(NULL) + MANUAL_DEFAULT_DURATION_SEC;
        }
        switch_zone(zone, until);
    }
}

static void handle_manual_start(uint8_t zone, uint32_t duration_sec)
{
    if (zone >= ZONE_COUNT) return;

    enter_mode(MODE_MANUAL);
    touch_manual_activity();

    time_t until = duration_sec > 0 ? time(NULL) + duration_sec : 0;
    switch_zone(zone, until);
}

static void handle_manual_stop(uint8_t zone)
{
    enter_mode(MODE_MANUAL);
    touch_manual_activity();

    uint8_t active = zone_get_active();
    if (zone == ZONE_NONE || active == zone)
    {
        switch_zone(ZONE_NONE, 0);
    }
}

static void handle_set_mode(controller_mode_t new_mode)
{
    if (new_mode == MODE_AUTO)
    {
        switch_zone(ZONE_NONE, 0);
        last_fired_minute = -1;
    }
    enter_mode(new_mode);
    if (new_mode == MODE_MANUAL) touch_manual_activity();
}

/* Periodic logic ************************************************************/

static void tick_auto(const struct tm* lt)
{
    int16_t minute_of_day = lt->tm_hour * 60 + lt->tm_min;
    if (minute_of_day == last_fired_minute) return;

    const schedule_entry_t* e =
        schedule_match(lt->tm_hour, lt->tm_min, lt->tm_wday);
    if (e)
    {
        ESP_LOGI(TAG,
                 "auto fire: zone=%u duration=%us",
                 e->zone,
                 e->duration_sec);
        time_t until = e->duration_sec > 0 ? time(NULL) + e->duration_sec : 0;
        switch_zone(e->zone, until);
        last_fired_minute = minute_of_day;
    }
}

static void tick_manual(time_t now)
{
    /* 30-min auto-return: only counts while no zone is running. Change the
     * condition below if the spec evolves (e.g. always count). */
    if (!zone_any_active() &&
        (now - last_manual_event) >= MANUAL_AUTORETURN_SEC)
    {
        ESP_LOGI(TAG, "manual idle timeout - returning to AUTO");
        enter_mode(MODE_AUTO);
    }
}

static void tick_run_until(time_t now)
{
    if (run_until > 0 && now >= run_until && zone_any_active())
    {
        ESP_LOGI(TAG, "run-until reached - stopping zone");
        switch_zone(ZONE_NONE, 0);
        if (mode == MODE_MANUAL) touch_manual_activity();
    }
}

static void tick(void)
{
    if (!time_sync_ready()) return;

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);

    tick_run_until(now);

    if (mode == MODE_AUTO)
        tick_auto(&lt);
    else
        tick_manual(now);
}

/* Task **********************************************************************/

static void task(void* arg)
{
    publish_mode();
    mqtt_publish_zone_state(ZONE_NONE, false);

    event_t ev;
    while (1)
    {
        if (xQueueReceive(queue, &ev, pdMS_TO_TICKS(TICK_MS)))
        {
            switch (ev.id)
            {
                case EV_BUTTON:
                    handle_button(ev.zone);
                    break;
                case EV_MANUAL_START:
                    handle_manual_start(ev.zone, ev.duration_sec);
                    break;
                case EV_MANUAL_STOP:
                    handle_manual_stop(ev.zone);
                    break;
                case EV_SET_MODE:
                    handle_set_mode(ev.mode);
                    break;
            }
        }
        tick();
    }
}

/* Public API ****************************************************************/

void controller_init(void)
{
    queue = xQueueCreate(16, sizeof(event_t));
    last_manual_event = time(NULL);
    xTaskCreate(task, "controller", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
}

void controller_on_button(uint8_t zone)
{
    event_t ev = {.id = EV_BUTTON, .zone = zone};
    xQueueSend(queue, &ev, 0);
}

void controller_set_mode(controller_mode_t new_mode)
{
    event_t ev = {.id = EV_SET_MODE, .mode = new_mode};
    xQueueSend(queue, &ev, 0);
}

void controller_manual_start(uint8_t zone, uint32_t duration_sec)
{
    event_t ev = {.id = EV_MANUAL_START,
                  .zone = zone,
                  .duration_sec = duration_sec};
    xQueueSend(queue, &ev, 0);
}

void controller_manual_stop(uint8_t zone)
{
    event_t ev = {.id = EV_MANUAL_STOP, .zone = zone};
    xQueueSend(queue, &ev, 0);
}

void controller_schedule_changed(void)
{ /* no-op for now */
}

void controller_publish_state(void)
{
    publish_mode();
    uint8_t z = zone_get_active();
    /* Publish all zones' current state (the inactive ones as OFF). */
    for (uint8_t i = 0; i < ZONE_COUNT; i++)
    {
        mqtt_publish_zone_state(i, i == z);
    }
}

controller_mode_t controller_get_mode(void) { return mode; }

uint8_t controller_get_active_zone(void) { return zone_get_active(); }
