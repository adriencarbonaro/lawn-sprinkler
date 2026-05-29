#include "button.h"
#include "config.h"
#include "controller.h"
#include "ha_entities.h"
#include "mqtt.h"
#include "schedule.h"
#include "sntp.h"
#include "version.h"
#include "wifi.h"
#include "zone.h"

static const ha_identity_t identity = {
    .device_id = CONFIG_DEVICE_ID,
    .device_name = CONFIG_DEVICE_NAME,
    .manufacturer = CONFIG_MANUFACTURER,
    .model = CONFIG_ESP32_MODEL,
    .version_str = DESCRIBE,
};

static void on_mqtt_connect(void) { controller_publish_state(); }

void app_main(void)
{
    /* Init Home Assistant layer */
    uint16_t nb_entities = 0;
    const ha_entity_t* entities = get_entities(&nb_entities);
    ha_init(&identity, entities, nb_entities);

    mqtt_set_on_connect(on_mqtt_connect);

    /* Init Wifi layer */
    wifi_init(mqtt_start, NULL);
    time_sync_init();

    /* User init */
    zone_init();
    controller_init();
    button_init();
    schedule_init();
}
