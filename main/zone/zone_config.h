#ifndef ZONE_CONFIG_H_
#define ZONE_CONFIG_H_

#include "sdkconfig.h"

#define ZONE_ACTIVE_LEVEL 1
#define ZONE_COUNT 6

/* After this much idle time in MANUAL with no zone active, return to AUTO */
#define AUTO_MODE_RESET_TIME_MIN CONFIG_AUTO_MODE_RESET_TIME_MIN

/* Duration applied to a manual button press, in seconds. 0 = no auto-stop. */
#define DEFAULT_DURATION_SEC CONFIG_DEFAULT_DURATION_SEC

#define GPIO_ZONE_1 CONFIG_GPIO_ZONE_1
#define GPIO_ZONE_2 CONFIG_GPIO_ZONE_2
#define GPIO_ZONE_3 CONFIG_GPIO_ZONE_3
#define GPIO_ZONE_4 CONFIG_GPIO_ZONE_4
#define GPIO_ZONE_5 CONFIG_GPIO_ZONE_5
#define GPIO_ZONE_6 CONFIG_GPIO_ZONE_6

#endif /* ZONE_CONFIG_H_ */
