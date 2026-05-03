#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MODE_AUTO,
    MODE_MANUAL,
} controller_mode_t;

void controller_init(void);

/* Inbound events. Safe to call from any task. */

void controller_on_button(uint8_t zone);
void controller_set_mode(controller_mode_t mode);

/* Start watering `zone` for `duration_sec` (0 = no auto-stop).
 * Forces MANUAL mode. */
void controller_manual_start(uint8_t zone, uint32_t duration_sec);
void controller_manual_stop(uint8_t zone);

/* Notify the controller that the schedule was replaced (no-op for now,
 * kept as a hook). */
void controller_schedule_changed(void);

/* Republish current mode + zone state. Called by mqtt.c after a (re)connect
 * so HA picks up the live state even if no retained messages are present. */
void controller_publish_state(void);

/* Read-only accessors (last published state). */
controller_mode_t controller_get_mode(void);
uint8_t controller_get_active_zone(void);

#endif /* CONTROLLER_H_ */
