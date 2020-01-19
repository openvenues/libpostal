#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/numex.h"

SUITE(libpostal_numex_tests);

static greatest_test_res test_numex(numex_table_t *numex_table, char *input, char *output, char *lang) {
    char *normalized = replace_numeric_expressions(numex_table, input, lang);

    if (normalized != NULL) {
        ASSERT_STR_EQ(output, normalized);
        free(normalized);
    } else {
        ASSERT_STR_EQ(output, input);
    }
    PASS();
}

TEST test_numeric_expressions(numex_table_t *numex_table) {

    // English numbers
    CHECK_CALL(test_numex(numex_table, "five hundred ninety-three", "593", "en"));
    CHECK_CALL(test_numex(numex_table, "five hundred and ninety-three", "593", "en"));
    CHECK_CALL(test_numex(numex_table, "fourth and a", "4th and a", "en"));
    CHECK_CALL(test_numex(numex_table, "foo and bar", "foo and bar", "en"));
    CHECK_CALL(test_numex(numex_table, "thirty west twenty-sixth street", "30 west 26th street", "en"));
    CHECK_CALL(test_numex(numex_table, "five and sixth", "5 and 6th", "en"));
    CHECK_CALL(test_numex(numex_table, "three hundred thousand nineteenhundred and forty-fifth", "301945th", "en"));
    CHECK_CALL(test_numex(numex_table, "seventeen eighty", "1780", "en"));
    CHECK_CALL(test_numex(numex_table, "ten oh four", "1004", "en"));
    CHECK_CALL(test_numex(numex_table, "ten and four", "10 and 4", "en"));

    // French (Celtic-style) numbers
    CHECK_CALL(test_numex(numex_table, "quatre-vingt-douze", "92", "fr"));
    CHECK_CALL(test_numex(numex_table, "quatre vingt douze", "92", "fr"));
    CHECK_CALL(test_numex(numex_table, "quatre vingts", "80", "fr"));
    CHECK_CALL(test_numex(numex_table, "soixante-et-onze", "71", "fr"));
    CHECK_CALL(test_numex(numex_table, "soixante-cinq", "65", "fr"));

    // French (Belgian/Swiss) numbers
    CHECK_CALL(test_numex(numex_table, "nonante-deux", "92", "fr"));
    CHECK_CALL(test_numex(numex_table, "septante-cinq", "75", "fr"));

    // German numbers
    CHECK_CALL(test_numex(numex_table, "sechs-und-fünfzig", "56", "de"));
    CHECK_CALL(test_numex(numex_table, "eins", "1", "de"));
    CHECK_CALL(test_numex(numex_table, "dreiundzwanzigste strasse", "23. strasse", "de"));

    // Italian numbers
    CHECK_CALL(test_numex(numex_table, "millenovecentonovantadue", "1992", "it"));
    CHECK_CALL(test_numex(numex_table, "ventiquattro", "24", "it"));


    // Spanish numbers
    CHECK_CALL(test_numex(numex_table, "tricentesima primera", "301.ª", "es"));

    // Roman numerals (la=Latin)

    CHECK_CALL(test_numex(numex_table, "via xx settembre", "via 20 settembre", "la"));
    CHECK_CALL(test_numex(numex_table, "mcccxlix anno domini", "1349 anno domini", "la"));
    CHECK_CALL(test_numex(numex_table, "str. st. nazionale dei giovi, milano", "str. st. nazionale dei giovi, milano", "la"));

    // Japanese numbers

    CHECK_CALL(test_numex(numex_table, "百二十", "120", "ja"));

    // Korean numbers

    CHECK_CALL(test_numex(numex_table, "천구백구십이", "1992", "ko"));

    PASS();
}

GREATEST_SUITE(libpostal_numex_tests) {
    numex_table_t *numex_table = numex_module_setup(DEFAULT_NUMEX_PATH);
    if (numex_table == NULL) {
        printf("Could not load numex module\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_numeric_expressions, numex_table);

    numex_module_teardown(&numex_table);
}
