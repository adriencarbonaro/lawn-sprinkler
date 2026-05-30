#include "led.h"
#include "led_config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.h"

static const char* TAG = "led";

static volatile status_led_state_t s_status = STATUS_LED_BOOTING;

static void status_set_level(bool on)
{
    gpio_set_level(GPIO_LED_STATUS, on ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL);
}

static void auto_set_level(bool on)
{
    gpio_set_level(GPIO_LED_AUTO, on ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL);
}

/* Sleep up to `ms`, waking early if the status changed in the meantime so
 * pattern transitions are reflected promptly (the slow blink's off phase is
 * several seconds long). Returns true if the status changed. */
static bool status_sleep(status_led_state_t expected, uint32_t ms)
{
    const uint32_t step_ms = 50;
    while (ms > 0)
    {
        uint32_t chunk = MIN(ms, step_ms);
        vTaskDelay(pdMS_TO_TICKS(chunk));
        if (s_status != expected) return true;
        ms -= chunk;
    }
    return false;
}

static void status_task(void* arg)
{
    while (1)
    {
        status_led_state_t s = s_status;
        switch (s)
        {
            case STATUS_LED_OFF:
                status_set_level(false);
                status_sleep(s, 200);
                break;

            case STATUS_LED_BOOTING:
                status_set_level(true);
                status_sleep(s, 200);
                break;

            case STATUS_LED_CONNECTING:
                status_set_level(true);
                if (status_sleep(s, STATUS_LED_CONNECTING_ON_MS)) break;
                status_set_level(false);
                status_sleep(s, STATUS_LED_CONNECTING_OFF_MS);
                break;

            case STATUS_LED_CONNECTED:
                status_set_level(true);
                if (status_sleep(s, STATUS_LED_CONNECTED_ON_MS)) break;
                status_set_level(false);
                status_sleep(s,
                             STATUS_LED_CONNECTED_PERIOD_MS -
                                 STATUS_LED_CONNECTED_ON_MS);
                break;
        }
    }
}

void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_LED_STATUS) | (1ULL << GPIO_LED_AUTO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    auto_set_level(false);
    s_status = STATUS_LED_BOOTING;
    status_set_level(true);

    xTaskCreate(status_task,
                "status_led",
                2048,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
}

void led_set_status(status_led_state_t state)
{
    ESP_LOGI(TAG, "status -> %d", (int)state);
    s_status = state;
}

void led_set_auto_mode(bool auto_on) { auto_set_level(auto_on); }
