#include "controller.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "ha.h"
#include "schedule.h"
#include "sntp.h"
#include "utils.h"
#include "zone.h"
#include "zone_config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define AUTO_MODE_RESET_TIME_MS (AUTO_MODE_RESET_TIME_MIN))

#define TICK_MS 500

/* Events ********************************************************************/

typedef enum
{
    EV_BUTTON,
    EV_SET_MODE,
    EV_SET_DURATION,
    EV_RUN_EXPIRED,
} event_id_t;

typedef struct
{
    event_id_t id;
    uint8_t zone;
    uint32_t duration_min;
    controller_mode_t mode;
} event_t;

#define DURATION_INFINITE 0

/* State
 *********************************************************************/

static const char* TAG = "controller";

static QueueHandle_t queue = NULL;

static controller_mode_t mode = MODE_AUTO;
static uint32_t duration_min = DEFAULT_DURATION_MIN;
static int16_t last_fired_minute = -1; /* hour*60+minute, dedup auto fires */

static TimerHandle_t idle_timer = NULL;
static TimerHandle_t run_timer = NULL;

/* Helpers *******************************************************************/

static void publish_mode(void) { ha_publish("mode", mode_str[mode]); }

static void publish_duration(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", (unsigned)duration_min);
    ha_publish("duration", buf);
}

static void publish_zone_state(uint8_t zone, bool on)
{
    char id[16];
    snprintf(id, sizeof(id), "zone_%u", zone);
    ha_publish(id, on ? "ON" : "OFF");
}

static void publish_zone_states(uint8_t prev_zone, uint8_t new_zone)
{
    if (prev_zone != new_zone)
    {
        if (prev_zone != ZONE_NONE) publish_zone_state(prev_zone, false);
        if (new_zone != ZONE_NONE) publish_zone_state(new_zone, true);
    }
}

static void update_idle_timer(void)
{
    if (!idle_timer) return;
    if (mode == MODE_MANUAL && !zone_any_active())
    {
        ESP_LOGI(TAG, "Starting idle timer (%u min)", AUTO_MODE_RESET_TIME_MIN);
        xTimerReset(idle_timer, 0);
    }
    else
    {
        ESP_LOGI(TAG, "Stoping idle timer");
        xTimerStop(idle_timer, 0);
    }
}

static void idle_timer_cb(TimerHandle_t t)
{
    event_t ev = {.id = EV_SET_MODE, .mode = MODE_AUTO};
    xQueueSend(queue, &ev, 0);
}

static void update_run_timer(uint32_t duration_min)
{
    if (!run_timer) return;
    if (duration_min > 0)
        xTimerChangePeriod(run_timer,
                           pdMS_TO_TICKS(1000 * TO_SEC(duration_min)),
                           0);
    else
        xTimerStop(run_timer, 0);
}

static void run_timer_cb(TimerHandle_t t)
{
    event_t ev = {.id = EV_RUN_EXPIRED};
    xQueueSend(queue, &ev, 0);
}

static void switch_zone(uint8_t new_zone, uint32_t duration_min)
{
    if (new_zone != ZONE_NONE)
        ESP_LOGI(TAG,
                 "Switching to zone %u for %u min",
                 new_zone,
                 duration_min);
    uint8_t prev = zone_get_active();
    zone_set_active(new_zone);
    update_run_timer(duration_min);
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

/* Event handlers ************************************************************/

static void handle_button(uint8_t zone)
{
    if (zone >= ZONE_COUNT) return;

    switch_mode(MODE_MANUAL);

    uint8_t active = zone_get_active();
    if (active == zone)
    {
        stop_all_zones();
    }
    else
    {
        switch_zone(zone, duration_min);
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
}

static void handle_set_duration(uint32_t min)
{
    duration_min = min;
    ESP_LOGI(TAG, "duration -> %u min", (unsigned)min);
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
                 "auto fire: zone=%u duration=%u min",
                 e->zone,
                 e->duration_min);
        switch_zone(e->zone, (uint32_t)e->duration_min);
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
                case EV_SET_MODE:
                    handle_set_mode(ev.mode);
                    break;
                case EV_SET_DURATION:
                    handle_set_duration(ev.duration_min);
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
    idle_timer =
        xTimerCreate("manual_idle",
                     pdMS_TO_TICKS(TO_MS(TO_SEC(AUTO_MODE_RESET_TIME_MIN))),
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

void controller_set_duration(uint32_t duration_min)
{
    event_t ev = {.id = EV_SET_DURATION, .duration_min = duration_min};
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
        publish_zone_state(i, i == z);
    }
}

controller_mode_t controller_get_mode(void) { return mode; }

uint8_t controller_get_active_zone(void) { return zone_get_active(); }
