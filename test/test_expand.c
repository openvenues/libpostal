#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/libpostal.h"

SUITE(libpostal_expansion_tests);

static greatest_test_res test_expansion_contains(char *input, char *output, normalize_options_t options) {
    size_t num_expansions;
    char **expansions = expand_address(input, options, &num_expansions);

    bool contains_expansion = false;
    char *expansion;
    for (size_t i = 0; i < num_expansions; i++) {
        expansion = expansions[i];
        if (strcmp(output, expansion) == 0) {
            contains_expansion = true;
            break;
        }

    }

    ASSERT(contains_expansion);
    PASS();
}

static greatest_test_res test_expansion_contains_with_languages(char *input, char *output, normalize_options_t options, size_t num_languages, ...) {
    char **languages = NULL;
    
    size_t i;

    if (num_languages > 0) {
        va_list args;

        va_start(args, num_languages);
        languages = malloc(sizeof(char *) * num_languages);
        char *lang;

        for (i = 0; i < num_languages; i++) {
            lang = va_arg(args, char *);
            ASSERT(strlen(lang) < MAX_LANGUAGE_LEN);
            languages[i] = strdup(lang);
        }

        va_end(args);

        options.num_languages = num_languages;
        options.languages = (char **)languages;
    } else {
        options.languages = NULL;
        options.num_languages = 0;
    }

    CHECK_CALL(test_expansion_contains(input, output, options));
    if (languages != NULL) {
        for (i = 0; i < num_languages; i++) {
            free(languages[i]);
        }
        free(languages);
    }
    PASS();
}


TEST test_expansions(void) {
    normalize_options_t options = get_libpostal_default_options();

    CHECK_CALL(test_expansion_contains_with_languages("123 Main St. #2f", "123 main street number 2f", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("S St. NW", "s street northwest", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("Marktstrasse", "markt strasse", options, 1, "de"));
    CHECK_CALL(test_expansion_contains_with_languages("Hoofdstraat", "hoofdstraat", options, 1, "nl"));
    CHECK_CALL(test_expansion_contains_with_languages("มงแตร", "มงแตร", options, 1, "th"));
    PASS();
}

TEST test_expansions_language_classifier(void) {
    normalize_options_t options = get_libpostal_default_options();

    CHECK_CALL(test_expansion_contains_with_languages("V XX Sett", "via 20 settembre", options, 0, NULL));
    CHECK_CALL(test_expansion_contains_with_languages("C/ Ocho", "calle 8", options, 0, NULL));
    PASS();
}


SUITE(libpostal_expansion_tests) {

    if (!libpostal_setup() || !libpostal_setup_language_classifier()) {
        printf("Could not setup libpostal\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_expansions);
    RUN_TEST(test_expansions_language_classifier);

    libpostal_teardown();
    libpostal_teardown_language_classifier();

}

