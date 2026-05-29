#include "ha_entities.h"

#include "controller.h"
#include "esp_log.h"
#include "ha.h"
#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char* TAG = "ha_entities";

/* Topics are relative to the device id; the ha layer prefixes "<device_id>/"
 * when building discovery and state/command topics. */
#define T_SET_KEYWORD "set"
#define T_STATE_KEYWORD "state"

#define T_MODE_STATE "mode/" T_STATE_KEYWORD
#define T_MODE_SET "mode/" T_SET_KEYWORD

#define T_DURATION_STATE "duration/" T_STATE_KEYWORD
#define T_DURATION_SET "duration/" T_SET_KEYWORD

#define T_ZONE_PREFIX "zone/"

/* Command callbacks (HA -> device) ******************************************/

static void on_mode_set(const char* id, const char* payload)
{
    (void)id;
    if (strcasecmp(payload, mode_str[MODE_AUTO]) == 0)
        controller_set_mode(MODE_AUTO);
    else if (strcasecmp(payload, mode_str[MODE_MANUAL]) == 0)
        controller_set_mode(MODE_MANUAL);
    else
        ESP_LOGW(TAG, "unknown mode payload: %s", payload);
}

static void on_duration_set(const char* id, const char* payload)
{
    (void)id;
    char* end = NULL;
    long min = strtol(payload, &end, 10);
    if (end == payload || min < 0)
    {
        ESP_LOGW(TAG, "duration: invalid payload: %s", payload);
        return;
    }
    controller_set_duration((uint32_t)min);
}

static void on_zone_set(const char* id, const char* payload)
{
    /* id is "zone_<n>"; the digit after the underscore is the zone index. */
    const char* p = strrchr(id, '_');
    if (p == NULL)
    {
        ESP_LOGW(TAG, "zone: malformed entity id: %s", id);
        return;
    }
    uint8_t zone = (uint8_t)atoi(p + 1);

    if (strcasecmp(payload, "ON") == 0 || strcasecmp(payload, "OFF") == 0)
        controller_on_button(zone);
    else
        ESP_LOGW(TAG, "zone %u set: unknown payload: %s", zone, payload);
}

/* Entity table **************************************************************/

#define ZONE_ENTITY(zone_index)                                       \
    {                                                                 \
        .id = "zone_" #zone_index, .name = "Zone " #zone_index,       \
        .platform = HA_SWITCH,                                        \
        .state_topic = T_ZONE_PREFIX #zone_index "/" T_STATE_KEYWORD, \
        .command_topic = T_ZONE_PREFIX #zone_index "/" T_SET_KEYWORD, \
        .icon = "mdi:sprinkler-variant",                              \
        .extra =                                                      \
            "\"payload_on\":\"ON\","                                  \
            "\"payload_off\":\"OFF\","                                \
            "\"state_on\":\"ON\","                                    \
            "\"state_off\":\"OFF\"",                                  \
        .on_command = on_zone_set,                                    \
    }

static const ha_entity_t s_entities[] = {
    {
        .id = "mode",
        .name = "Mode",
        .platform = HA_SELECT,
        .state_topic = T_MODE_STATE,
        .command_topic = T_MODE_SET,
        .icon = "mdi:auto-mode",
        .extra = "\"options\":[\"AUTO\",\"MANUAL\"]",
        .on_command = on_mode_set,
    },
    {
        .id = "duration",
        .name = "Duration",
        .platform = HA_NUMBER,
        .state_topic = T_DURATION_STATE,
        .command_topic = T_DURATION_SET,
        .icon = "mdi:timer-sand",
        .extra = "\"min\":0,\"max\":60,\"step\":1,"
                 "\"unit_of_measurement\":\"min\","
                 "\"mode\":\"box\"",
        .on_command = on_duration_set,
    },
    ZONE_ENTITY(0),
    ZONE_ENTITY(1),
    ZONE_ENTITY(2),
    ZONE_ENTITY(3),
    ZONE_ENTITY(4),
    ZONE_ENTITY(5),
};

const ha_entity_t* get_entities(uint16_t* nb_entities)
{
    *nb_entities = ARRAY_DIM(s_entities);
    return s_entities;
}
