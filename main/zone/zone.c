#include "zone.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "zone";

static const gpio_num_t zone_gpios[ZONE_COUNT] = ZONE_GPIOS;
static uint8_t active_zone = ZONE_NONE;

static void apply(uint8_t index, bool on)
{
    int level = on ? ZONE_ACTIVE_LEVEL : !ZONE_ACTIVE_LEVEL;
    gpio_set_level(zone_gpios[index], level);
}

void zone_init(void)
{
    uint64_t mask = 0;
    for (uint8_t i = 0; i < ZONE_COUNT; i++)
    {
        mask |= 1ULL << zone_gpios[i];
    }

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    for (uint8_t i = 0; i < ZONE_COUNT; i++) apply(i, false);
    active_zone = ZONE_NONE;
}

void zone_set_active(uint8_t index)
{
    if (index != ZONE_NONE && index >= ZONE_COUNT)
    {
        ESP_LOGW(TAG, "invalid zone index: %u", index);
        return;
    }

    if (index == active_zone) return;

    if (active_zone != ZONE_NONE) apply(active_zone, false);
    if (index != ZONE_NONE) apply(index, true);

    active_zone = index;
}

uint8_t zone_get_active(void) { return active_zone; }

bool zone_any_active(void) { return active_zone != ZONE_NONE; }
