#ifndef MQTT_TOPICS_INTERNAL_H_
#define MQTT_TOPICS_INTERNAL_H_

#include "config.h"

/* Base topic for this device (set in Kconfig as MQTT_TOPIC_PREFIX, e.g.
 * "lawn-sprinkler/"). Always ends with '/'. */
#define T_BASE MQTT_TOPIC_PREFIX

#define T_AVAIL T_BASE "availability"
#define T_VERSION T_BASE "version"

#define T_SET_KEYWORD "set"
#define T_STATE_KEYWORD "state"

#define T_MODE_STATE T_BASE "mode/" T_STATE_KEYWORD
#define T_MODE_SET T_BASE "mode/" T_SET_KEYWORD

#define T_DURATION_STATE T_BASE "duration/" T_STATE_KEYWORD
#define T_DURATION_SET T_BASE "duration/" T_SET_KEYWORD

#define T_SCHED_STATE T_BASE "schedule/" T_STATE_KEYWORD
#define T_SCHED_SET T_BASE "schedule/" T_SET_KEYWORD

#define T_ZONE_PREFIX T_BASE "zone/"

#endif /* MQTT_TOPICS_INTERNAL_H_ */
