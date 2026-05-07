#include "mqtt_discovery.h"

#include "config.h"
#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_topics_internal.h"
#include "version.h"
#include "zone_config.h"

#include <stdio.h>
#include <stdlib.h>

static const char* TAG = "mqtt_discovery";

#define DISCOVERY_PREFIX "homeassistant"
#define DISCOVERY_TOPIC DISCOVERY_PREFIX "/device/" DEVICE_ID "/config"

#define DEVICE_BLOCK                                \
    "\"device\":{"                                  \
    "\"identifiers\":[\"" DEVICE_ID                 \
    "\"],"                                          \
    "\"name\":\"" DEVICE_NAME                       \
    "\","                                           \
    "\"manufacturer\":\"" MANUFACTURER              \
    "\","                                           \
    "\"model\":\"" ESP32_MODEL                      \
    "\","                                           \
    "\"sw_version\":\"" VERSION " (" BUILD_ID_SHORT \
    ")\""                                           \
    "}"

#define ORIGIN_BLOCK            \
    "\"origin\":{"              \
    "\"name\":\"" DEVICE_NAME   \
    "\","                       \
    "\"sw_version\":\"" VERSION \
    "\""                        \
    "}"

#define AVAIL_BLOCK                         \
    "\"availability\":[{"                   \
    "\"topic\":\"" T_AVAIL                  \
    "\","                                   \
    "\"payload_available\":\"online\","     \
    "\"payload_not_available\":\"offline\"" \
    "}]"

#define MODE_COMPONENT                   \
    "\"" DEVICE_ID                       \
    "_mode\":{"                          \
    "\"platform\":\"select\","           \
    "\"name\":\"Mode\","                 \
    "\"unique_id\":\"" DEVICE_ID         \
    "_mode\","                           \
    "\"object_id\":\"" DEVICE_ID         \
    "_mode\","                           \
    "\"command_topic\":\"" T_MODE_SET    \
    "\","                                \
    "\"state_topic\":\"" T_MODE_STATE    \
    "\","                                \
    "\"options\":[\"AUTO\",\"MANUAL\"]," \
    "\"icon\":\"mdi:auto-mode\""         \
    "}"

#define VERSION_COMPONENT                 \
    "\"" DEVICE_ID                        \
    "_version\":{"                        \
    "\"platform\":\"sensor\","            \
    "\"name\":\"Version\","               \
    "\"unique_id\":\"" DEVICE_ID          \
    "_version\","                         \
    "\"object_id\":\"" DEVICE_ID          \
    "_version\","                         \
    "\"state_topic\":\"" T_VERSION        \
    "\","                                 \
    "\"entity_category\":\"diagnostic\"," \
    "\"icon\":\"mdi:git\""                \
    "}"

static int append_zone(char* buf, size_t cap, size_t off, uint8_t i)
{
    return snprintf(buf + off,
                    cap - off,
                    ",\"" DEVICE_ID
                    "_zone_%u\":{"
                    "\"platform\":\"switch\","
                    "\"name\":\"Zone %u\","
                    "\"unique_id\":\"" DEVICE_ID
                    "_zone_%u\","
                    "\"object_id\":\"" DEVICE_ID
                    "_zone_%u\","
                    "\"command_topic\":\"" T_ZONE_PREFIX
                    "%u/set\","
                    "\"state_topic\":\"" T_ZONE_PREFIX
                    "%u/state\","
                    "\"payload_on\":\"ON\","
                    "\"payload_off\":\"OFF\","
                    "\"state_on\":\"ON\","
                    "\"state_off\":\"OFF\","
                    "\"icon\":\"mdi:sprinkler-variant\""
                    "}",
                    i,
                    i + 1,
                    i,
                    i,
                    i,
                    i);
}

void mqtt_discovery_publish_all(void)
{
    ESP_LOGI(TAG, "publishing HA device discovery");

    size_t cap = 4096;
    char* payload = malloc(cap);
    if (!payload)
    {
        ESP_LOGE(TAG, "discovery: out of memory");
        return;
    }

    int off =
        snprintf(payload,
                 cap,
                 "{" DEVICE_BLOCK "," ORIGIN_BLOCK "," AVAIL_BLOCK
                 ",\"components\":{" MODE_COMPONENT "," VERSION_COMPONENT);
    if (off < 0 || (size_t)off >= cap) goto truncated;

    for (uint8_t i = 0; i < ZONE_COUNT; i++)
    {
        int n = append_zone(payload, cap, (size_t)off, i);
        if (n < 0 || (size_t)(off + n) >= cap) goto truncated;
        off += n;
    }

    int n = snprintf(payload + off, cap - off, "}}");
    if (n < 0 || (size_t)(off + n) >= cap) goto truncated;

    mqtt_publish(DISCOVERY_TOPIC, payload, 1);
    free(payload);
    return;

truncated:
    ESP_LOGE(TAG, "discovery payload truncated");
    free(payload);
}
