#include "led.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "led";

static const gpio_num_t led_gpios[LED_COUNT] = LED_GPIOS;

static void apply(uint8_t index, bool on)
{
    int level = on ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL;
    gpio_set_level(led_gpios[index], level);
}

void led_init(void)
{
    uint64_t mask = 0;
    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        mask |= 1ULL << led_gpios[i];
    }

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    for (uint8_t i = 0; i < LED_COUNT; i++) apply(i, false);
    ESP_LOGI(TAG, "led init ok");
}

void led_set_zone(uint8_t zone)
{
    for (uint8_t i = 0; i < LED_COUNT; i++)
    {
        apply(i, i == zone);
    }
}
