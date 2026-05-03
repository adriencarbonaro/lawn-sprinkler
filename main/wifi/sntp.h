#ifndef SNTP_H_
#define SNTP_H_

#include <stdbool.h>

void time_sync_init(void);

bool time_sync_ready(void);

#endif /* SNTP_H_ */
