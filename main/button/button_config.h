#ifndef BUTTON_CONFIG_H_
#define BUTTON_CONFIG_H_

#include "zone_config.h"

#define BUTTON_COUNT ZONE_COUNT

#define BUTTON_ACTIVE_LEVEL 0

#define BUTTON_LONG_PRESS_TIME 1000
#define BUTTON_SHORT_PRESS_TIME 200

/* Placeholder GPIOs - update with the final schematic. */
#define BUTTON_GPIOS                                                          \
    {                                                                         \
        4, /* zone 1 */                                                       \
        5, /* zone 2 */                                                       \
        6, /* zone 3 */                                                       \
        7, /* zone 4 */                                                       \
        8, /* zone 5 */                                                       \
        9, /* zone 6 */                                                       \
    }

#endif /* BUTTON_CONFIG_H_ */
