#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/transliterate.h"

SUITE(libpostal_transliteration_tests);

static greatest_test_res test_transliteration(char *trans_name, char *input, char *output) {
    char *transliterated = transliterate(trans_name, input, strlen(input));

    ASSERT_STR_EQ(output, transliterated);
    free(transliterated);
    PASS();
}

TEST test_transliterators(void) {
    CHECK_CALL(test_transliteration("greek-latin", "διαφορετικούς", "diaphoretikoús̱"));
    CHECK_CALL(test_transliteration("devanagari-latin", "ज़", "za"));
    CHECK_CALL(test_transliteration("arabic-latin", "شارع", "sẖạrʿ"));
    CHECK_CALL(test_transliteration("cyrillic-latin", "улица", "ulica"));
    CHECK_CALL(test_transliteration("russian-latin-bgn", "улица", "ulitsa"));
    CHECK_CALL(test_transliteration("hebrew-latin", "רחוב", "rẖwb"));
    CHECK_CALL(test_transliteration("latin-ascii", "foo &amp; bar", "foo & bar"));
    CHECK_CALL(test_transliteration("latin-ascii-simple", "eschenbräu bräurei triftstraße 67½ &amp; foo", "eschenbräu bräurei triftstraße 67½ & foo"));
    CHECK_CALL(test_transliteration("han-latin", "街𠀀abcdef", "jiēhēabcdef"));
    CHECK_CALL(test_transliteration("katakana-latin", "ドウ", "dou"));
    CHECK_CALL(test_transliteration("hiragana-latin", "どう", "dou"));
    CHECK_CALL(test_transliteration("latin-ascii-simple", "at&t", "at&t"));
    CHECK_CALL(test_transliteration("latin-ascii-simple", "at&amp;t", "at&t"));

    PASS();
}

GREATEST_SUITE(libpostal_transliteration_tests) {
    if (!transliteration_module_setup(DEFAULT_TRANSLITERATION_PATH)) {
        printf("Could not load transliterator module\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_transliterators);

    transliteration_module_teardown();
}
