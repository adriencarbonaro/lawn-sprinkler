#ifndef UTILS_H_
#define UTILS_H_

#define ARRAY_DIM(x) (sizeof(x) / sizeof((x)[0]))

#define XSTR(x) #x
#define STR(x) XSTR(x)

#define HI_BYTE(x) ((x) >> 8) & 0xff
#define LO_BYTE(x) (x) & 0xff

#define TO_SEC(min) (min) * 60
#define TO_MS(secs) (secs) * 1000

#endif /* UTILS_H_ */
