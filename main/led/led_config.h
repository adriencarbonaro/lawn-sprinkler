#ifndef LED_CONFIG_H_
#define LED_CONFIG_H_

#include "zone_config.h"

#define LED_COUNT ZONE_COUNT

#define LED_ACTIVE_LEVEL 1

/* Placeholder GPIOs - update with the final schematic. */
#define LED_GPIOS                                                             \
    {                                                                         \
        18, /* zone 1 */                                                      \
        19, /* zone 2 */                                                      \
        20, /* zone 3 */                                                      \
        21, /* zone 4 */                                                      \
        22, /* zone 5 */                                                      \
        23, /* zone 6 */                                                      \
    }

#endif /* LED_CONFIG_H_ */
