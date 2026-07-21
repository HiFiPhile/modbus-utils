#ifndef MBU_COMMON_H
#define MBU_COMMON_H

#include <stdint.h>

#ifndef MBU_VERSION
#define MBU_VERSION "development"
#endif

#define MBU_MAX_SLAVE_ADDRESS 247
#define MBU_MAX_DATA_ADDRESS 65535

int mbu_parse_int(const char *text, int minimum, int maximum, int *value);
int mbu_parse_address_range(const char *text, int *start, int *end);
int mbu_timeout_from_ms(int milliseconds, uint32_t *seconds, uint32_t *microseconds);
void mbu_sleep_ms(unsigned int milliseconds);

#endif
