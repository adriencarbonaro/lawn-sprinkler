#include "mqtt.h"

#include "config.h"
#include "controller.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_discovery.h"
#include "mqtt_topics_internal.h"
#include "version.h"
#include "zone_config.h"

#include <string.h>

static const char* TAG = "mqtt";

static esp_mqtt_client_handle_t client = NULL;
static volatile bool connected = false;
static volatile bool started = false;

static void on_event(void* arg,
                     esp_event_base_t base,
                     int32_t id,
                     void* event_data)
{
    esp_mqtt_event_handle_t e = event_data;

    switch ((esp_mqtt_event_id_t)id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "connected");
            connected = true;
            mqtt_topics_subscribe(e->client);
            mqtt_publish_availability("online");
            mqtt_publish(T_VERSION, VERSION " (" BUILD_ID_SHORT ")", 1);
            mqtt_discovery_publish_all();
            controller_publish_state();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "disconnected");
            connected = false;
            break;

        case MQTT_EVENT_DATA:
            mqtt_topics_dispatch(e->topic, e->topic_len, e->data, e->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "error event");
            break;

        default:
            break;
    }
}

bool mqtt_is_connected(void) { return connected; }

int mqtt_publish(const char* topic, const char* msg, int retain)
{
    if (!client || !connected) return -1;
    return esp_mqtt_client_publish(client,
                                   topic,
                                   msg,
                                   msg ? strlen(msg) : 0,
                                   1,
                                   retain);
}

void mqtt_publish_availability(const char* status)
{
    mqtt_publish(T_AVAIL, status, 1);
}

void mqtt_publish_mode(const char* mode_str)
{
    mqtt_publish(T_MODE_STATE, mode_str, 1);
}

void mqtt_publish_duration(uint32_t duration_min)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", (unsigned)duration_min);
    mqtt_publish(T_DURATION_STATE, buf, 1);
}

void mqtt_publish_zone_state(uint8_t zone, bool on)
{
    char topic[64];
    if (zone == 0xff)
    {
        /* publish all zones off */
        for (uint8_t i = 0; i < ZONE_COUNT; i++)
        {
            snprintf(topic, sizeof(topic), T_ZONE_PREFIX "%u/state", i);
            mqtt_publish(topic, "OFF", 1);
        }
        return;
    }

    snprintf(topic, sizeof(topic), T_ZONE_PREFIX "%u/state", zone);
    mqtt_publish(topic, on ? "ON" : "OFF", 1);
}

void mqtt_publish_schedule_json(const char* json)
{
    mqtt_publish(T_SCHED_STATE, json, 1);
}

static void on_ip_event(void* arg,
                        esp_event_base_t base,
                        int32_t id,
                        void* data)
{
    if (id == IP_EVENT_STA_GOT_IP && client && !started)
    {
        ESP_LOGI(TAG, "got IP - starting mqtt client");
        esp_mqtt_client_start(client);
        started = true;
    }
}

void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
        .session.keepalive = 60,
        .session.last_will.topic = T_AVAIL,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .network.reconnect_timeout_ms = 5000,
    };

    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, on_event, client);

    /* Defer the actual TCP connect until Wi-Fi has an IP - otherwise the
     * first connect attempt fails with "Host is unreachable". */
    esp_event_handler_register(IP_EVENT,
                               IP_EVENT_STA_GOT_IP,
                               &on_ip_event,
                               NULL);
}
