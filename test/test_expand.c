#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/libpostal.h"

SUITE(libpostal_expansion_tests);

static greatest_test_res test_expansion_contains(char *input, char *output, libpostal_normalize_options_t options) {
    size_t num_expansions;
    char **expansions = libpostal_expand_address(input, options, &num_expansions);

    bool contains_expansion = false;
    char *expansion;
    for (size_t i = 0; i < num_expansions; i++) {
        expansion = expansions[i];
        if (string_equals(output, expansion)) {
            contains_expansion = true;
            break;
        }

    }

    libpostal_expansion_array_destroy(expansions, num_expansions);

    if (!contains_expansion) {
        printf("Expansions should contain %s, got {", output);
        for (size_t i = 0; i < num_expansions; i++) {
            expansion = expansions[i];
            printf("%s%s", expansion, i < num_expansions - 1 ? "," : "");
        }
        printf("}\n");
        FAIL();
    }

    PASS();
}

static greatest_test_res test_expansion_contains_with_languages(char *input, char *output, libpostal_normalize_options_t options, size_t num_languages, ...) {
    char **languages = NULL;
    
    size_t i;

    if (num_languages > 0) {
        va_list args;

        va_start(args, num_languages);
        languages = malloc(sizeof(char *) * num_languages);
        char *lang;

        for (i = 0; i < num_languages; i++) {
            lang = va_arg(args, char *);
            ASSERT(strlen(lang) < LIBPOSTAL_MAX_LANGUAGE_LEN);
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
    libpostal_normalize_options_t options = libpostal_get_default_options();

    CHECK_CALL(test_expansion_contains_with_languages("123 Main St. #2f", "123 main street number 2f", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("120 E 96th St", "120 east 96 street", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("120 E Ninety-sixth St", "120 east 96 street", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("4998 Vanderbilt Dr, Columbus, OH 43213", "4998 vanderbilt drive columbus ohio 43213", options, 1, "en");
    CHECK_CALL(test_expansion_contains_with_languages("S St. NW", "s street northwest", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages("Marktstrasse", "markt strasse", options, 1, "de"));
    CHECK_CALL(test_expansion_contains_with_languages("Hoofdstraat", "hoofdstraat", options, 1, "nl"));
    CHECK_CALL(test_expansion_contains_with_languages("มงแตร", "มงแตร", options, 1, "th"));
    PASS();
}

TEST test_expansions_language_classifier(void) {
    libpostal_normalize_options_t options = libpostal_get_default_options();

    CHECK_CALL(test_expansion_contains_with_languages("V XX Sett", "via 20 settembre", options, 0, NULL));
    CHECK_CALL(test_expansion_contains_with_languages("C/ Ocho", "calle 8", options, 0, NULL));
    PASS();
}

TEST test_expansions_no_options(void) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.lowercase = false;
    options.latin_ascii = false;
    options.transliterate = false;
    options.strip_accents = false;
    options.decompose = false;
    options.trim_string = false;
    options.drop_parentheticals = false;
    options.replace_numeric_hyphens = false;
    options.delete_numeric_hyphens = false;
    options.split_alpha_from_numeric = false;
    options.replace_word_hyphens = false;
    options.delete_word_hyphens = false;
    options.delete_final_periods = false;
    options.delete_acronym_periods = false;
    options.drop_english_possessives = false;
    options.delete_apostrophes = false;
    options.expand_numex = false;
    options.roman_numerals = false;

    CHECK_CALL(test_expansion_contains_with_languages("120 E 96th St New York", "120 E 96th St New York", options, 0, NULL));
    PASS();
}


SUITE(libpostal_expansion_tests) {
    if (!libpostal_setup() || !libpostal_setup_language_classifier()) {
        printf("Could not setup libpostal\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_expansions);
    RUN_TEST(test_expansions_language_classifier);
    RUN_TEST(test_expansions_no_options);

    libpostal_teardown();
    libpostal_teardown_language_classifier();

}

