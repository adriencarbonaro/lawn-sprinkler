#include "mqtt_discovery.h"

#include "config.h"
#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_topics_internal.h"
#include "version.h"
#include "zone_config.h"

#include <stdio.h>

static const char* TAG = "mqtt_discovery";

#define DISCOVERY_PREFIX "homeassistant"

/* Embed the device block as a substring inside each component config. */
#define DEVICE_BLOCK                   \
    "\"device\":{"                     \
    "\"identifiers\":[\"" DEVICE_ID    \
    "\"],"                             \
    "\"name\":\"" DEVICE_NAME          \
    "\","                              \
    "\"manufacturer\":\"" MANUFACTURER \
    "\","                              \
    "\"model\":\"" ESP32_MODEL         \
    "\""                               \
    ","                                \
    "\"sw_version\":\"" VERSION        \
    "\""                               \
    "}"

#define AVAIL_BLOCK                     \
    "\"availability_topic\":\"" T_AVAIL \
    "\","                               \
    "\"payload_available\":\"online\"," \
    "\"payload_not_available\":\"offline\""

static void publish_zone(uint8_t i)
{
    char topic[96];
    char payload[640];

    snprintf(topic,
             sizeof(topic),
             DISCOVERY_PREFIX "/switch/" DEVICE_ID "/zone_%u/config",
             i);

    snprintf(payload,
             sizeof(payload),
             "{"
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
             "\"icon\":\"mdi:sprinkler-variant\"," AVAIL_BLOCK "," DEVICE_BLOCK
             "}",
             i + 1,
             i,
             i,
             i,
             i);

    mqtt_publish(topic, payload, 1);
}

static void publish_mode_select(void)
{
    const char* topic = DISCOVERY_PREFIX "/select/" DEVICE_ID "/mode/config";

    const char* payload =
        "{"
        "\"name\":\"Mode\","
        "\"unique_id\":\"" DEVICE_ID
        "_mode\","
        "\"object_id\":\"" DEVICE_ID
        "_mode\","
        "\"command_topic\":\"" T_MODE_SET
        "\","
        "\"state_topic\":\"" T_MODE_STATE
        "\","
        "\"options\":[\"AUTO\",\"MANUAL\"],"
        "\"icon\":\"mdi:auto-mode\"," AVAIL_BLOCK "," DEVICE_BLOCK "}";

    mqtt_publish(topic, payload, 1);
}

void mqtt_discovery_publish_all(void)
{
    ESP_LOGI(TAG, "publishing HA discovery");
    publish_mode_select();
    for (uint8_t i = 0; i < ZONE_COUNT; i++) publish_zone(i);
}
