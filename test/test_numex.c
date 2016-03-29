#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/numex.h"

SUITE(libpostal_numex_tests);

static greatest_test_res test_numex(char *input, char *output, char *lang) {
    char *normalized = replace_numeric_expressions(input, lang);

    if (normalized != NULL) {
        ASSERT_STR_EQ(output, normalized);
        free(normalized);
    } else {
        ASSERT_STR_EQ(output, input);
    }
    PASS();
}

TEST test_numeric_expressions(void) {

    // English numbers
    CHECK_CALL(test_numex("five hundred ninety-three", "593", "en"));
    CHECK_CALL(test_numex("five hundred and ninety-three", "593", "en"));
    CHECK_CALL(test_numex("fourth and a", "4th and a", "en"));
    CHECK_CALL(test_numex("foo and bar", "foo and bar", "en"));
    CHECK_CALL(test_numex("thirty west twenty-sixth street", "30 west 26th street", "en"));
    CHECK_CALL(test_numex("five and sixth", "5 and 6th", "en"));
    CHECK_CALL(test_numex("three hundred thousand nineteenhundred and forty-fifth", "301945th", "en"));
    CHECK_CALL(test_numex("seventeen eighty", "1780", "en"));
    CHECK_CALL(test_numex("ten oh four", "1004", "en"));
    CHECK_CALL(test_numex("ten and four", "10 and 4", "en"));

    // French (Celtic-style) numbers
    CHECK_CALL(test_numex("quatre-vingt-douze", "92", "fr"));
    CHECK_CALL(test_numex("quatre vingt douze", "92", "fr"));
    CHECK_CALL(test_numex("quatre vingts", "80", "fr"));
    CHECK_CALL(test_numex("soixante-et-onze", "71", "fr"));
    CHECK_CALL(test_numex("soixante-cinq", "65", "fr"));

    // French (Belgian/Swiss) numbers
    CHECK_CALL(test_numex("nonante-deux", "92", "fr"));
    CHECK_CALL(test_numex("septante-cinq", "75", "fr"));

    // German numbers
    CHECK_CALL(test_numex("sechs-und-fünfzig", "56", "de"));
    CHECK_CALL(test_numex("eins", "1", "de"));
    CHECK_CALL(test_numex("dreiundzwanzigste strasse", "23. strasse", "de"));

    // Italian numbers
    CHECK_CALL(test_numex("millenovecentonovantadue", "1992", "it"));
    CHECK_CALL(test_numex("ventiquattro", "24", "it"));


    // Spanish numbers
    CHECK_CALL(test_numex("tricentesima primera", "301.ª", "es"));

    // Roman numerals (la=Latin)

    CHECK_CALL(test_numex("via xx settembre", "via 20 settembre", "la"));
    CHECK_CALL(test_numex("mcccxlix anno domini", "1349 anno domini", "la"));
    CHECK_CALL(test_numex("str. st. nazionale dei giovi, milano", "str. st. nazionale dei giovi, milano", "la"));

    // Japanese numbers

    CHECK_CALL(test_numex("百二十", "120", "ja"));

    // Korean numbers

    CHECK_CALL(test_numex("천구백구십이", "1992", "ko"));

    PASS();
}

GREATEST_SUITE(libpostal_numex_tests) {
    if (!numex_module_setup(DEFAULT_NUMEX_PATH)) {
        printf("Could not load numex module\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_numeric_expressions);

    numex_module_teardown();
}
