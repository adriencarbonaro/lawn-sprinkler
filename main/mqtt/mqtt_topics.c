#include "cJSON.h"
#include "controller.h"
#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_topics_internal.h"
#include "schedule.h"
#include "zone_config.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "mqtt_topics";

static bool topic_eq(const char* topic, int topic_len, const char* expected)
{
    int len = (int)strlen(expected);
    return topic_len == len && memcmp(topic, expected, len) == 0;
}

static bool starts_with(const char* topic,
                        int topic_len,
                        const char* prefix,
                        int* out_remaining)
{
    int len = (int)strlen(prefix);
    if (topic_len < len) return false;
    if (memcmp(topic, prefix, len) != 0) return false;
    *out_remaining = len;
    return true;
}

static bool payload_eq_ci(const char* msg, int msg_len, const char* expected)
{
    int len = (int)strlen(expected);
    if (msg_len != len) return false;
    for (int i = 0; i < len; i++)
    {
        char a = msg[i];
        char b = expected[i];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) return false;
    }
    return true;
}

static void publish_schedule_state(void)
{
    const schedule_t* s = schedule_get();
    cJSON* arr = cJSON_CreateArray();
    for (uint8_t i = 0; i < s->count; i++)
    {
        const schedule_entry_t* e = &s->entries[i];
        cJSON* o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "zone", e->zone);
        cJSON_AddNumberToObject(o, "hour", e->hour);
        cJSON_AddNumberToObject(o, "minute", e->minute);
        cJSON_AddNumberToObject(o, "dow", e->dow_mask);
        cJSON_AddNumberToObject(o, "duration", e->duration_min);
        cJSON_AddBoolToObject(o, "enabled", e->enabled);
        cJSON_AddItemToArray(arr, o);
    }
    char* json = cJSON_PrintUnformatted(arr);
    if (json)
    {
        mqtt_publish_schedule_json(json);
        free(json);
    }
    cJSON_Delete(arr);
}

static void handle_mode_set(const char* msg, int msg_len)
{
    if (payload_eq_ci(msg, msg_len, mode_str[MODE_AUTO]))
        controller_set_mode(MODE_AUTO);
    else if (payload_eq_ci(msg, msg_len, mode_str[MODE_MANUAL]))
        controller_set_mode(MODE_MANUAL);
    else
        ESP_LOGW(TAG, "unknown mode payload: %.*s", msg_len, msg);
}

static void handle_duration_set(const char* msg, int msg_len)
{
    char buf[16] = {0};
    int n = MIN(msg_len, (int)sizeof(buf) - 1);
    memcpy(buf, msg, n);
    char* end = NULL;
    long secs = strtol(buf, &end, 10);
    if (end == buf || secs < 0)
    {
        ESP_LOGW(TAG, "duration: invalid payload: %.*s", msg_len, msg);
        return;
    }
    controller_set_duration((uint32_t)secs);
}

static void handle_schedule_set(const char* msg, int msg_len)
{
    cJSON* root = cJSON_ParseWithLength(msg, msg_len);
    if (!root || !cJSON_IsArray(root))
    {
        ESP_LOGW(TAG, "schedule: invalid JSON");
        cJSON_Delete(root);
        return;
    }

    schedule_t s = {0};
    int n = cJSON_GetArraySize(root);
    for (int i = 0; i < n && s.count < SCHEDULE_MAX_ENTRIES; i++)
    {
        cJSON* o = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(o)) continue;

        cJSON* zone = cJSON_GetObjectItem(o, "zone");
        cJSON* hour = cJSON_GetObjectItem(o, "hour");
        cJSON* minute = cJSON_GetObjectItem(o, "minute");
        cJSON* dow = cJSON_GetObjectItem(o, "dow");
        cJSON* dur = cJSON_GetObjectItem(o, "duration");
        cJSON* en = cJSON_GetObjectItem(o, "enabled");

        if (!cJSON_IsNumber(zone) || !cJSON_IsNumber(hour) ||
            !cJSON_IsNumber(minute) || !cJSON_IsNumber(dow) ||
            !cJSON_IsNumber(dur))
            continue;
        if (zone->valueint < 0 || zone->valueint >= ZONE_COUNT) continue;

        schedule_entry_t* e = &s.entries[s.count++];
        e->zone = (uint8_t)zone->valueint;
        e->hour = (uint8_t)hour->valueint;
        e->minute = (uint8_t)minute->valueint;
        e->dow_mask = (uint8_t)dow->valueint;
        e->duration_min = (uint16_t)dur->valueint;
        e->enabled = en ? cJSON_IsTrue(en) : true;
    }

    cJSON_Delete(root);

    schedule_replace(&s);
    controller_schedule_changed();
    publish_schedule_state();
}

static uint8_t parse_zone_topic(const char* sub,
                                int sub_len,
                                const char** suffix_out,
                                int* suffix_len_out)
{
    int i = 0;
    int value = 0;
    while (i < sub_len && sub[i] >= '0' && sub[i] <= '9')
    {
        value = value * 10 + (sub[i] - '0');
        i++;
    }
    if (i == 0 || i >= sub_len || sub[i] != '/') return 0xff;
    if (value < 0 || value >= ZONE_COUNT) return 0xff;

    *suffix_out = &sub[i + 1];
    *suffix_len_out = sub_len - (i + 1);
    return (uint8_t)value;
}

static void handle_zone_topic(const char* sub,
                              int sub_len,
                              const char* msg,
                              int msg_len)
{
    const char* suffix = NULL;
    int suffix_len = 0;
    uint8_t zone = parse_zone_topic(sub, sub_len, &suffix, &suffix_len);
    if (zone == 0xff) return;

    if (suffix_len == 3 && memcmp(suffix, "set", 3) == 0)
    {
        if (payload_eq_ci(msg, msg_len, "ON") ||
            payload_eq_ci(msg, msg_len, "OFF"))
            controller_on_button(zone);
        else
            ESP_LOGW(TAG, "zone %u set: unknown payload", zone);
    }
}

void mqtt_topics_dispatch(const char* topic,
                          int topic_len,
                          const char* msg,
                          int msg_len)
{
    ESP_LOGI(TAG, "rx %.*s: %.*s", topic_len, topic, msg_len, msg);

    if (topic_eq(topic, topic_len, T_MODE_SET))
    {
        handle_mode_set(msg, msg_len);
        return;
    }
    if (topic_eq(topic, topic_len, T_DURATION_SET))
    {
        handle_duration_set(msg, msg_len);
        return;
    }
    if (topic_eq(topic, topic_len, T_SCHED_SET))
    {
        handle_schedule_set(msg, msg_len);
        return;
    }

    int off = 0;
    if (starts_with(topic, topic_len, T_ZONE_PREFIX, &off))
    {
        handle_zone_topic(topic + off, topic_len - off, msg, msg_len);
        return;
    }

    ESP_LOGW(TAG, "unhandled topic: %.*s", topic_len, topic);
}

void mqtt_topics_subscribe(esp_mqtt_client_handle_t c)
{
    esp_mqtt_client_subscribe(c, T_MODE_SET, 1);
    esp_mqtt_client_subscribe(c, T_DURATION_SET, 1);
    esp_mqtt_client_subscribe(c, T_SCHED_SET, 1);
    esp_mqtt_client_subscribe(c, T_ZONE_PREFIX "+/set", 1);

    /* Re-publish current schedule on connect. */
    publish_schedule_state();
}
