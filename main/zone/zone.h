#ifndef ZONE_H_
#define ZONE_H_

#include <stdbool.h>
#include <stdint.h>

#define ZONE_NONE 0xff

void zone_init(void);

/* Activate zone `index` (0..ZONE_COUNT-1). Any other active zone is
 * deactivated first - only one zone runs at a time. Pass ZONE_NONE to stop
 * all zones. */
void zone_set_active(uint8_t index);

uint8_t zone_get_active(void);

/* Returns true if at least one zone is currently on. */
bool zone_any_active(void);

#endif /* ZONE_H_ */
