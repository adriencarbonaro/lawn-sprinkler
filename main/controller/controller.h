#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MODE_AUTO,
    MODE_MANUAL,
} controller_mode_t;

static const char* mode_str[] = {
    [MODE_AUTO] = "AUTO",
    [MODE_MANUAL] = "MANUAL",
};

void controller_init(void);

/* Inbound events. Safe to call from any task. */

void controller_on_button(uint8_t zone);
void controller_set_mode(controller_mode_t mode);
void controller_set_duration(uint32_t duration_min);

/* Republish current mode + zone state. Wired as the mqtt on-connect callback
 * so HA picks up the live state even if no retained messages are present. */
void controller_publish_state(void);

/* Read-only accessors (last published state). */
controller_mode_t controller_get_mode(void);
uint8_t controller_get_active_zone(void);

#endif /* CONTROLLER_H_ */
