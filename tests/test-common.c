#include "mbu-common.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_integer_parsing(void) {
    int value;

    assert(mbu_parse_int("42", 0, 100, &value) == 0 && value == 42);
    assert(mbu_parse_int("0x2a", 0, 100, &value) == 0 && value == 42);
    assert(mbu_parse_int("08", 0, 100, &value) == 0 && value == 8);
    assert(mbu_parse_int("-1", -2, 2, &value) == 0 && value == -1);
    assert(mbu_parse_int("", 0, 100, &value) == -1);
    assert(mbu_parse_int("12x", 0, 100, &value) == -1);
    assert(mbu_parse_int("101", 0, 100, &value) == -1);
    assert(mbu_parse_int("1", 2, 1, &value) == -1);
}

static void test_address_ranges(void) {
    int start;
    int end;

    assert(mbu_parse_address_range("12", &start, &end) == 0 && start == 12 && end == 12);
    assert(mbu_parse_address_range("1.10", &start, &end) == 0 && start == 1 && end == 10);
    assert(mbu_parse_address_range("0.247", &start, &end) == 0 && start == 0 && end == 247);
    assert(mbu_parse_address_range("10.1", &start, &end) == -1);
    assert(mbu_parse_address_range("1.1", &start, &end) == -1);
    assert(mbu_parse_address_range("248", &start, &end) == -1);
    assert(mbu_parse_address_range("1.2.3", &start, &end) == -1);
}

static void test_timeouts(void) {
    uint32_t seconds;
    uint32_t microseconds;

    assert(mbu_timeout_from_ms(1, &seconds, &microseconds) == 0 && seconds == 0 &&
           microseconds == 1000);
    assert(mbu_timeout_from_ms(1000, &seconds, &microseconds) == 0 && seconds == 1 &&
           microseconds == 0);
    assert(mbu_timeout_from_ms(1250, &seconds, &microseconds) == 0 && seconds == 1 &&
           microseconds == 250000);
    assert(mbu_timeout_from_ms(0, &seconds, &microseconds) == -1);
}

int main(void) {
    test_integer_parsing();
    test_address_ranges();
    test_timeouts();
    puts("common tests: PASS");
    return 0;
}
