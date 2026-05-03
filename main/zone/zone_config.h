#ifndef ZONE_CONFIG_H_
#define ZONE_CONFIG_H_

#define ZONE_COUNT 6

/* Triac drivers (MOC3041) are active-high. Pins are placeholders -
 * update once the schematic is final. */
#define ZONE_ACTIVE_LEVEL 1

#define ZONE_GPIOS                                                            \
    {                                                                         \
        0, /* zone 1 */                                                       \
        1, /* zone 2 */                                                       \
        2, /* zone 3 */                                                       \
        3, /* zone 4 */                                                       \
        10, /* zone 5 */                                                      \
        11, /* zone 6 */                                                      \
    }

#endif /* ZONE_CONFIG_H_ */
