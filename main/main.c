#include "button.h"
#include "config.h"
#include "controller.h"
#include "ha_entities.h"
#include "led.h"
#include "mqtt.h"
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

static void on_wifi_connect(void) { mqtt_start(); }

static void on_wifi_disconnect(void)
{
    /* Covers both retries and a sustained inability to connect. */
    led_set_status(STATUS_LED_CONNECTING);
}

static void on_mqtt_connect(void)
{
    led_set_status(STATUS_LED_CONNECTED);
    controller_publish_state();
}

void app_main(void)
{
    /* Status LED solid while we bring the device up. */
    led_init();

    /* Init Home Assistant layer */
    uint16_t nb_entities = 0;
    const ha_entity_t* entities = get_entities(&nb_entities);
    ha_init(&identity, entities, nb_entities);

    mqtt_set_on_connect(on_mqtt_connect);

    /* User init */
    zone_init();
    controller_init();
    button_init();

    led_set_status(STATUS_LED_CONNECTING);
    wifi_init(on_wifi_connect, on_wifi_disconnect);
}
