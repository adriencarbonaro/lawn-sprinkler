#include "controller.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "mqtt.h"
#include "schedule.h"
#include "sntp.h"
#include "utils.h"
#include "zone.h"
#include "zone_config.h"

#include <string.h>
#include <time.h>

/* Tunables ******************************************************************/

/* After this much idle time in MANUAL with no zone active, return to AUTO.
 * Change here if the spec evolves. */
#define MANUAL_AUTORETURN_SEC (30 * 60)

#define TICK_MS 500

/* Events ********************************************************************/

typedef enum
{
    EV_BUTTON,
    EV_MANUAL_START,
    EV_MANUAL_STOP,
    EV_SET_MODE,
    EV_SET_DURATION,
    EV_RUN_EXPIRED,
} event_id_t;

typedef struct
{
    event_id_t id;
    uint8_t zone;
    uint32_t duration_sec;
    controller_mode_t mode;
} event_t;

typedef enum
{
    DURATION_0 = CONFIG_DURATION_0,
    DURATION_1 = CONFIG_DURATION_1,
    DURATION_2 = CONFIG_DURATION_2,
    DURATION_3 = CONFIG_DURATION_3,
} duration_t;

static const char* duration_str[] = {
    [DURATION_0] = DURATION_0_STR,
    [DURATION_1] = DURATION_1_STR,
    [DURATION_2] = DURATION_2_STR,
    [DURATION_3] = DURATION_3_STR,
};

#define DURATION_DEFAULT DURATION_0
#define DURATION_INFINITE 0

/* State
 *********************************************************************/

static const char* TAG = "controller";

static QueueHandle_t queue = NULL;

static controller_mode_t mode = MODE_AUTO;
static duration_t duration = DURATION_DEFAULT;
static int16_t last_fired_minute = -1; /* hour*60+minute, dedup auto fires */

static TimerHandle_t idle_timer = NULL;
static TimerHandle_t run_timer = NULL;

/* Helpers *******************************************************************/

static void publish_mode(void) { mqtt_publish_mode(mode_str[mode]); }
static void publish_duration(void)
{
    mqtt_publish_duration(duration_str[duration]);
}

static void publish_zone_states(uint8_t prev_zone, uint8_t new_zone)
{
    if (prev_zone != new_zone)
    {
        if (prev_zone != ZONE_NONE) mqtt_publish_zone_state(prev_zone, false);
        if (new_zone != ZONE_NONE) mqtt_publish_zone_state(new_zone, true);
    }
}

static void update_idle_timer(void)
{
    if (!idle_timer) return;
    if (mode == MODE_MANUAL && !zone_any_active())
        xTimerReset(idle_timer, 0);
    else
        xTimerStop(idle_timer, 0);
}

static void idle_timer_cb(TimerHandle_t t)
{
    event_t ev = {.id = EV_SET_MODE, .mode = MODE_AUTO};
    xQueueSend(queue, &ev, 0);
}

static void update_run_timer(uint32_t duration_sec)
{
    if (!run_timer) return;
    if (duration_sec > 0)
        xTimerChangePeriod(run_timer, pdMS_TO_TICKS(1000) * duration_sec, 0);
    else
        xTimerStop(run_timer, 0);
}

static void run_timer_cb(TimerHandle_t t)
{
    event_t ev = {.id = EV_RUN_EXPIRED};
    xQueueSend(queue, &ev, 0);
}

static void switch_zone(uint8_t new_zone, uint32_t duration)
{
    if (new_zone != ZONE_NONE)
        ESP_LOGI(TAG, "Switching to zone %u for %u secs", new_zone, duration);
    uint8_t prev = zone_get_active();
    zone_set_active(new_zone);
    update_run_timer(duration);
    publish_zone_states(prev, new_zone);
    update_idle_timer();
}

static void stop_all_zones(void) { switch_zone(ZONE_NONE, DURATION_INFINITE); }

static void switch_mode(controller_mode_t new_mode)
{
    if (new_mode == mode) return;
    mode = new_mode;
    ESP_LOGI(TAG, "Switching to mode %s", mode_str[mode]);
    publish_mode();
    update_idle_timer();
}

static inline void trigger_manual_activity(void) { update_idle_timer(); }

/* Event handlers ************************************************************/

static void handle_button(uint8_t zone)
{
    if (zone >= ZONE_COUNT) return;

    switch_mode(MODE_MANUAL);
    trigger_manual_activity();

    uint8_t active = zone_get_active();
    if (active == zone)
    {
        stop_all_zones();
    }
    else
    {
        switch_zone(zone, duration);
    }
}

static void handle_manual_start(uint8_t zone, uint32_t duration_sec)
{
    if (zone >= ZONE_COUNT) return;

    switch_mode(MODE_MANUAL);
    trigger_manual_activity();

    switch_zone(zone, duration_sec);
}

static void handle_manual_stop(uint8_t zone)
{
    switch_mode(MODE_MANUAL);
    trigger_manual_activity();

    uint8_t active = zone_get_active();
    if (zone == ZONE_NONE || active == zone)
    {
        stop_all_zones();
    }
}

static void handle_set_mode(controller_mode_t new_mode)
{
    if (new_mode == MODE_AUTO)
    {
        stop_all_zones();
        last_fired_minute = -1;
    }
    switch_mode(new_mode);
    if (new_mode == MODE_MANUAL) trigger_manual_activity();
}

static void handle_set_duration(uint32_t sec)
{
    switch (sec)
    {
        case DURATION_0:
        case DURATION_1:
        case DURATION_2:
        case DURATION_3:
            duration = (duration_t)sec;
            ESP_LOGI(TAG, "duration -> %us", (unsigned)sec);
            break;
        default:
            ESP_LOGW(TAG, "duration: rejected %us", (unsigned)sec);
            break;
    }
    publish_duration();
}

static void handle_run_expired(void)
{
    if (!zone_any_active()) return;
    ESP_LOGI(TAG, "Stopping zones (timer reached)");
    stop_all_zones();
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
        switch_zone(e->zone, e->duration_sec);
        last_fired_minute = minute_of_day;
    }
}

static void tick(void)
{
    if (!time_sync_ready()) return;
    if (mode != MODE_AUTO) return;

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    tick_auto(&lt);
}

/* Task **********************************************************************/

static void task(void* arg)
{
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
                case EV_SET_DURATION:
                    handle_set_duration(ev.duration_sec);
                    break;
                case EV_RUN_EXPIRED:
                    handle_run_expired();
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
    idle_timer = xTimerCreate("manual_idle",
                              pdMS_TO_TICKS(MANUAL_AUTORETURN_SEC * 1000),
                              pdFALSE,
                              NULL,
                              idle_timer_cb);
    run_timer = xTimerCreate("zone_run",
                             pdMS_TO_TICKS(1000),
                             pdFALSE,
                             NULL,
                             run_timer_cb);
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

void controller_set_duration(uint32_t duration_sec)
{
    event_t ev = {.id = EV_SET_DURATION, .duration_sec = duration_sec};
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
    ESP_LOGI(TAG, "Publishing states");
    publish_mode();
    publish_duration();
    uint8_t z = zone_get_active();
    /* Publish all zones' current state (the inactive ones as OFF). */
    for (uint8_t i = 0; i < ZONE_COUNT; i++)
    {
        mqtt_publish_zone_state(i, i == z);
    }
}

controller_mode_t controller_get_mode(void) { return mode; }

uint8_t controller_get_active_zone(void) { return zone_get_active(); }
