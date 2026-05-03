#ifndef MQTT_TOPICS_INTERNAL_H_
#define MQTT_TOPICS_INTERNAL_H_

#include "config.h"

/* Base topic for this device (set in Kconfig as MQTT_TOPIC_PREFIX, e.g.
 * "lawn-sprinkler/"). Always ends with '/'. */
#define T_BASE MQTT_TOPIC_PREFIX

#define T_AVAIL T_BASE "availability"
#define T_VERSION T_BASE MQTT_TOPIC_VERSION

#define T_MODE_STATE T_BASE "mode/state"
#define T_MODE_SET T_BASE "mode/set"

#define T_SCHED_STATE T_BASE "schedule/state"
#define T_SCHED_SET T_BASE "schedule/set"

#define T_ZONE_PREFIX T_BASE "zone/"
/* Per-zone leaves: zone/<n>/state, zone/<n>/set, zone/<n>/run */

#endif /* MQTT_TOPICS_INTERNAL_H_ */
