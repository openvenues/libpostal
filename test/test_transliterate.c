#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/transliterate.h"

SUITE(libpostal_transliteration_tests);

static greatest_test_res test_transliteration(transliteration_table_t *trans_table, char *trans_name, char *input, char *output) {
    char *transliterated = transliterate(trans_table, trans_name, input, strlen(input));

    ASSERT_STR_EQ(output, transliterated);
    free(transliterated);
    PASS();
}

TEST test_transliterators(transliteration_table_t *trans_table) {
    CHECK_CALL(test_transliteration(trans_table, "greek-latin", "διαφορετικούς", "diaphoretikoús̱"));
    CHECK_CALL(test_transliteration(trans_table, "devanagari-latin", "ज़", "za"));
    CHECK_CALL(test_transliteration(trans_table, "arabic-latin", "شارع", "sẖạrʿ"));
    CHECK_CALL(test_transliteration(trans_table, "cyrillic-latin", "улица", "ulica"));
    CHECK_CALL(test_transliteration(trans_table, "russian-latin-bgn", "улица", "ulitsa"));
    CHECK_CALL(test_transliteration(trans_table, "hebrew-latin", "רחוב", "rẖwb"));
    CHECK_CALL(test_transliteration(trans_table, "latin-ascii", "foo &amp; bar", "foo & bar"));
    CHECK_CALL(test_transliteration(trans_table, "latin-ascii-simple", "eschenbräu bräurei triftstraße 67½ &amp; foo", "eschenbräu bräurei triftstraße 67½ & foo"));
    CHECK_CALL(test_transliteration(trans_table, "han-latin", "街𠀀abcdef", "jiēhēabcdef"));
    CHECK_CALL(test_transliteration(trans_table, "katakana-latin", "ドウ", "dou"));
    CHECK_CALL(test_transliteration(trans_table, "hiragana-latin", "どう", "dou"));
    CHECK_CALL(test_transliteration(trans_table, "latin-ascii-simple", "at&t", "at&t"));
    CHECK_CALL(test_transliteration(trans_table, "latin-ascii-simple", "at&amp;t", "at&t"));

    PASS();
}

GREATEST_SUITE(libpostal_transliteration_tests) {
    transliteration_table_t *trans_table = transliteration_module_setup(DEFAULT_TRANSLITERATION_PATH);
    if (trans_table == NULL) {
        printf("Could not load transliterator module\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_transliterators, trans_table);

    transliteration_module_teardown(&trans_table);
}
