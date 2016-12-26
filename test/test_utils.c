#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"

#include "../config.h"
#include "../src/libpostal.h"

SUITE(libpostal_util_tests);

TEST test_version(void) {
    const char *version = libpostal_version();
    ASSERT(version != NULL);
    ASSERT_STR_EQ(PACKAGE_VERSION, version);
    PASS();
}

GREATEST_SUITE(libpostal_util_tests) {
    RUN_TEST(test_version);
}
