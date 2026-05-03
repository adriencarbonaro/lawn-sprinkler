#include "schedule.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

#define NVS_NAMESPACE "sprinkler"
#define NVS_KEY_SCHEDULE "schedule"

static const char* TAG = "schedule";

static schedule_t cache = {0};

static void load_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "nvs open ro failed (%s) - using empty schedule",
                 esp_err_to_name(err));
        cache.count = 0;
        return;
    }

    size_t size = sizeof(cache);
    err = nvs_get_blob(h, NVS_KEY_SCHEDULE, &cache, &size);
    nvs_close(h);

    if (err != ESP_OK || size != sizeof(cache))
    {
        ESP_LOGI(TAG, "no stored schedule (%s) - using empty",
                 esp_err_to_name(err));
        memset(&cache, 0, sizeof(cache));
    }
    else
    {
        ESP_LOGI(TAG, "loaded %u schedule entries", cache.count);
    }
}

static void save_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs open rw failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(h, NVS_KEY_SCHEDULE, &cache, sizeof(cache));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "schedule persisted (%u entries)", cache.count);
    }
}

void schedule_init(void) { load_from_nvs(); }

const schedule_t* schedule_get(void) { return &cache; }

void schedule_replace(const schedule_t* sched)
{
    if (!sched) return;
    if (sched->count > SCHEDULE_MAX_ENTRIES) return;

    memcpy(&cache, sched, sizeof(cache));
    save_to_nvs();
}

const schedule_entry_t* schedule_match(uint8_t hour,
                                       uint8_t minute,
                                       uint8_t dow)
{
    uint8_t dow_bit = 1 << dow;
    for (uint8_t i = 0; i < cache.count; i++)
    {
        const schedule_entry_t* e = &cache.entries[i];
        if (!e->enabled) continue;
        if (e->hour != hour) continue;
        if (e->minute != minute) continue;
        if (!(e->dow_mask & dow_bit)) continue;
        return e;
    }
    return NULL;
}
