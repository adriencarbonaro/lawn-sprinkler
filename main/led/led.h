#ifndef LED_H_
#define LED_H_

#include <stdint.h>

#include "led_config.h"

#define LED_NONE 0xff

void led_init(void);

/* Light the LED for the given zone (0..LED_COUNT-1) and turn the others off.
 * Pass LED_NONE to turn all LEDs off. */
void led_set_zone(uint8_t zone);

#endif /* LED_H_ */
