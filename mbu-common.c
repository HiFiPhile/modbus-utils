#include "mbu-common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static int parse_number(const char *text, char **end, long *value) {
    int base = 10;

    if (text == NULL || *text == '\0') {
        return -1;
    }

    if ((text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) ||
        ((text[0] == '+' || text[0] == '-') && text[1] == '0' &&
         (text[2] == 'x' || text[2] == 'X'))) {
        base = 16;
    }

    errno = 0;
    *value = strtol(text, end, base);
    if (errno == ERANGE || *end == text) {
        return -1;
    }
    return 0;
}

int mbu_parse_int(const char *text, int minimum, int maximum, int *value) {
    char *end;
    long parsed;

    if (value == NULL || minimum > maximum || parse_number(text, &end, &parsed) == -1 ||
        *end != '\0' || parsed < minimum || parsed > maximum || parsed < INT_MIN ||
        parsed > INT_MAX) {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

int mbu_parse_address_range(const char *text, int *start, int *end) {
    char *separator;
    char *range_end;
    long first;
    long last;

    if (start == NULL || end == NULL || parse_number(text, &separator, &first) == -1 || first < 0 ||
        first > MBU_MAX_SLAVE_ADDRESS) {
        return -1;
    }

    if (*separator == '\0') {
        *start = (int)first;
        *end = (int)first;
        return 0;
    }
    if (*separator != '.' || parse_number(separator + 1, &range_end, &last) == -1 ||
        *range_end != '\0' || last < 0 || last > MBU_MAX_SLAVE_ADDRESS || first >= last) {
        return -1;
    }

    *start = (int)first;
    *end = (int)last;
    return 0;
}

int mbu_timeout_from_ms(int milliseconds, uint32_t *seconds, uint32_t *microseconds) {
    if (milliseconds <= 0 || seconds == NULL || microseconds == NULL) {
        return -1;
    }

    *seconds = (uint32_t)(milliseconds / 1000);
    *microseconds = (uint32_t)(milliseconds % 1000) * 1000U;
    return 0;
}

void mbu_sleep_ms(unsigned int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec request;
    struct timespec remaining;

    request.tv_sec = (time_t)(milliseconds / 1000U);
    request.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&request, &remaining) == -1 && errno == EINTR) {
        request = remaining;
    }
#endif
}
