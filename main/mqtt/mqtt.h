#ifndef MQTT_H_
#define MQTT_H_

#include <stdbool.h>
#include <stdint.h>

#include "mqtt_client.h"

void mqtt_init(void);

bool mqtt_is_connected(void);

/* Generic publish. retain=1 for state/discovery topics. */
int mqtt_publish(const char* topic, const char* msg, int retain);

/* Convenience helpers. */
void mqtt_publish_zone_state(uint8_t zone, bool on);
void mqtt_publish_mode(const char* mode_str);
void mqtt_publish_schedule_json(const char* json);
void mqtt_publish_availability(const char* status);

/* Topic dispatcher entry point - called by mqtt.c on incoming messages. */
void mqtt_topics_dispatch(const char* topic,
                          int topic_len,
                          const char* msg,
                          int msg_len);

/* Subscribe to all topics this device cares about. Called once on connect. */
void mqtt_topics_subscribe(esp_mqtt_client_handle_t client);

#endif /* MQTT_H_ */
