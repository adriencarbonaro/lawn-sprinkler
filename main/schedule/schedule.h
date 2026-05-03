#ifndef SCHEDULE_H_
#define SCHEDULE_H_

#include <stdbool.h>
#include <stdint.h>

#include "zone_config.h"

#define SCHEDULE_MAX_ENTRIES 32

/* Days-of-week mask: bit 0 = Sunday ... bit 6 = Saturday */
#define DOW_SUN (1 << 0)
#define DOW_MON (1 << 1)
#define DOW_TUE (1 << 2)
#define DOW_WED (1 << 3)
#define DOW_THU (1 << 4)
#define DOW_FRI (1 << 5)
#define DOW_SAT (1 << 6)
#define DOW_ALL 0x7f

typedef struct
{
    bool enabled;
    uint8_t zone;          /* 0..ZONE_COUNT-1 */
    uint8_t hour;          /* 0..23 */
    uint8_t minute;        /* 0..59 */
    uint8_t dow_mask;      /* DOW_* bitmask */
    uint16_t duration_sec; /* watering duration in seconds */
} schedule_entry_t;

typedef struct
{
    uint8_t count;
    schedule_entry_t entries[SCHEDULE_MAX_ENTRIES];
} schedule_t;

void schedule_init(void);

const schedule_t* schedule_get(void);

/* Replace the full schedule and persist to NVS. */
void schedule_replace(const schedule_t* sched);

/* Returns the entry that should fire at the given local time, or NULL if
 * none. `now` is the time-of-day in seconds since midnight. The dow check
 * uses tm_wday from the broken-down time. */
const schedule_entry_t* schedule_match(uint8_t hour,
                                       uint8_t minute,
                                       uint8_t dow);

#endif /* SCHEDULE_H_ */
