#ifndef LED_H_
#define LED_H_

#include <stdbool.h>

typedef enum
{
    STATUS_LED_OFF,
    STATUS_LED_BOOTING,
    STATUS_LED_CONNECTING,
    STATUS_LED_CONNECTED,
} status_led_state_t;

void led_init(void);

/* Set the status LED pattern. */
void led_set_status(status_led_state_t state);

/* Auto-mode LED: on in AUTO, off in MANUAL. */
void led_set_auto_mode(bool auto_on);

#endif /* LED_H_ */
