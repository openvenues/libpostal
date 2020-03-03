#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/string_utils.h"
#include "../src/libpostal.h"

SUITE(libpostal_expansion_tests);

static greatest_test_res test_expansion_contains_phrase_option(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options, bool root) {
    size_t num_expansions;

    char **expansions = NULL;
    if (!root) {
        expansions = libpostal_expand_address(classifier, instance, input, options, &num_expansions);
    } else {
        expansions = libpostal_expand_address_root(classifier, instance, input, options, &num_expansions);
    }

    bool contains_expansion = false;
    char *expansion;
    for (size_t i = 0; i < num_expansions; i++) {
        expansion = expansions[i];
        printf("expansion = %s\n", expansion);
        if (string_equals(output, expansion)) {
            contains_expansion = true;
            break;
        }

    }

    if (!contains_expansion) {
        printf("Expansions should contain %s, got {", output);
        for (size_t i = 0; i < num_expansions; i++) {
            expansion = expansions[i];
            printf("%s%s", expansion, i < num_expansions - 1 ? "," : "");
        }
        printf("}\n");
        FAIL();
    }

    libpostal_expansion_array_destroy(expansions, num_expansions);

    PASS();
}

static greatest_test_res test_expansion_contains(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options) {
    bool root = false;
    CHECK_CALL(test_expansion_contains_phrase_option(instance, classifier, input, output, options, root));

    PASS();
}

static greatest_test_res test_root_expansion_contains(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options) {
    bool root = true;
    CHECK_CALL(test_expansion_contains_phrase_option(instance, classifier, input, output, options, root));

    PASS();
}

static greatest_test_res test_expansion_contains_phrase_option_with_languages(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options, bool root, size_t num_languages, va_list args) {
    char **languages = NULL;
    
    size_t i;

    if (num_languages > 0) {
        languages = malloc(sizeof(char *) * num_languages);
        char *lang;

        for (i = 0; i < num_languages; i++) {
            lang = va_arg(args, char *);
            ASSERT(strlen(lang) < LIBPOSTAL_MAX_LANGUAGE_LEN);
            languages[i] = strdup(lang);
        }

        options.num_languages = num_languages;
        options.languages = (char **)languages;
    } else {
        options.languages = NULL;
        options.num_languages = 0;
    }

    CHECK_CALL(test_expansion_contains_phrase_option(instance, classifier, input, output, options, root));
    if (languages != NULL) {
        for (i = 0; i < num_languages; i++) {
            free(languages[i]);
        }
        free(languages);
    }
    PASS();
}



static greatest_test_res test_expansion_contains_with_languages(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options, size_t num_languages, ...) {
    bool root = false;
    va_list args;
    if (num_languages > 0) {
        va_start(args, num_languages);
        CHECK_CALL(test_expansion_contains_phrase_option_with_languages(instance, classifier, input, output, options, root, num_languages, args));
        va_end(args);
    } else {
        CHECK_CALL(test_expansion_contains_phrase_option_with_languages(instance, classifier, input, output, options, root, num_languages, args));
    }
    PASS();
}


static greatest_test_res test_root_expansion_contains_with_languages(libpostal_t *instance, language_classifier_t *classifier, char *input, char *output, libpostal_normalize_options_t options, size_t num_languages, ...) {
   bool root = true;
   va_list args;
   if (num_languages > 0) {
        va_start(args, num_languages);
        CHECK_CALL(test_expansion_contains_phrase_option_with_languages(instance, classifier, input, output, options, root, num_languages, args));
        va_end(args);
    } else {
        CHECK_CALL(test_expansion_contains_phrase_option_with_languages(instance, classifier, input, output, options, root, num_languages, args));
    }
    PASS();
}



TEST test_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();

    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "123 Main St. #2f", "123 main street number 2f", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "120 E 96th St", "120 east 96 street", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "120 E Ninety-sixth St", "120 east 96 street", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "4998 Vanderbilt Dr, Columbus, OH 43213", "4998 vanderbilt drive columbus ohio 43213", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Nineteen oh one W El Segundo Blvd", "1901 west el segundo boulevard", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "S St. NW", "s street northwest", options, 1, "en"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Quatre vingt douze Ave des Champs-Élysées", "92 avenue des champs-elysees", options, 1, "fr"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Quatre vingt douze Ave des Champs-Élysées", "92 avenue des champs elysees", options, 1, "fr"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Quatre vingt douze Ave des Champs-Élysées", "92 avenue des champselysees", options, 1, "fr"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Marktstrasse", "markt strasse", options, 1, "de"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "Hoofdstraat", "hoofdstraat", options, 1, "nl"));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "มงแตร", "มงแตร", options, 1, "th"));

    PASS();
}

TEST test_expansion_for_non_address_input(libpostal_t *instance, language_classifier_t *classifier) {
    size_t num_expansions;

    // This is tested as the input caused a segfault in expand_alternative_phrase_option
    char **expansions = libpostal_expand_address(classifier, instance, "ida-b@wells.co", libpostal_get_default_options(), &num_expansions);
    libpostal_expansion_array_destroy(expansions, num_expansions);
    PASS();
}

TEST test_street_root_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.address_components = LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_ANY;

    // English - normal cases
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Malcolm X Blvd", "malcolm x", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E 106th St", "106", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "S Park Ave", "park", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Park South", "park", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Rev Dr. MLK Dr S", "martin luther king junior", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Rev Dr. Martin Luther King Jr Dr S", "martin luther king junior", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "East 6th Street", "6th", options));

    // English - edge cases
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue B", "b", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue C", "c", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue D", "d", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue E", "e", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue N", "n", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "U St SE", "u", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "S Park", "park", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Park S", "park", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Avenue Rd", "avenue", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Broadway", "broadway", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E Broadway", "broadway", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E Center St", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E Ctr St", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E Center Street", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "E Ctr Street", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Center St E", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Ctr St E", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Center Street E", "center", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "Ctr Street E", "center", options));

    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "W. UNION STREET", "union", options, 2, "en", "es"));


    // Spanish
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "C/ Ocho", "8", options));
    PASS();
}


TEST test_house_number_root_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.address_components = LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_ANY;

    // English - normal cases
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "1A", "1 a", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "A1", "a 1", options));
    CHECK_CALL(test_root_expansion_contains(instance, classifier, "1", "1", options));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "# 1", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "No. 1", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "House No. 1", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "House #1", "1", options, 1, "en"));

    PASS();
}

TEST test_level_root_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.address_components = LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ANY;

    // English - normal cases
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1st Fl", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1st Floor", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "First Fl", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "First Floor", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "2nd Fl", "2", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "2nd Floor", "2", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Second Fl", "2", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Second Floor", "2", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Fl #1", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Fl No. 1", "1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Floor No. 1", "1", options, 1, "en"));

    // Specifiers
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "SB 1", "sub basement 1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Bsmt", "basement", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Bsmt 1", "basement 1", options, 1, "en"));

    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1G", "1 ground", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "G", "ground", options, 1, "en"));

    PASS();
}

TEST test_unit_root_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.address_components = LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_ANY;

    // English - normal cases
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1A", "1 a", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "A1", "a 1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Apt 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Apt No 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Apt #101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Apartment 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Apartment #101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Ste 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Ste No 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Ste #101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Suite 101", "101", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Suite #101", "101", options, 1, "en"));

    // Specifiers
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "PH 1", "penthouse 1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "PH1", "penthouse 1", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "Penthouse 1", "penthouse 1", options, 1, "en"));

    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1L", "1l", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1L", "1 left", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1F", "1f", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1F", "1f", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1R", "1r", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "1R", "1r", options, 1, "en"));

    PASS();
}


TEST test_po_box_root_expansions(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();
    options.address_components = LIBPOSTAL_ADDRESS_PO_BOX | LIBPOSTAL_ADDRESS_ANY;

    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "PO Box 1234", "1234", options, 1, "en"));
    CHECK_CALL(test_root_expansion_contains_with_languages(instance, classifier, "PO Box #1234", "1234", options, 1, "en"));

    PASS();
}

TEST test_expansions_language_classifier(libpostal_t *instance, language_classifier_t *classifier) {
    libpostal_normalize_options_t options = libpostal_get_default_options();

    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "V XX Sett", "via 20 settembre", options, 0, NULL));
    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "C/ Ocho", "calle 8", options, 0, NULL));
    PASS();
}

TEST test_expansions_no_options(libpostal_t *instance, language_classifier_t *classifier) {
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

    CHECK_CALL(test_expansion_contains_with_languages(instance, classifier, "120 E 96th St New York", "120 E 96th St New York", options, 0, NULL));
    PASS();
}


SUITE(libpostal_expansion_tests) {
    libpostal_t *instance = libpostal_setup();
    language_classifier_t *classifier = libpostal_setup_language_classifier();

    if (instance == NULL || classifier == NULL) {
        printf("Could not setup libpostal\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_expansions, instance, classifier);
    RUN_TEST(test_street_root_expansions, instance, classifier);
    RUN_TEST(test_house_number_root_expansions, instance, classifier);
    RUN_TEST(test_level_root_expansions, instance, classifier);
    RUN_TEST(test_unit_root_expansions, instance, classifier);
    RUN_TEST(test_po_box_root_expansions, instance, classifier);
    RUN_TEST(test_expansions_language_classifier, instance, classifier);
    RUN_TEST(test_expansions_no_options, instance, classifier);
    RUN_TEST(test_expansion_for_non_address_input, instance, classifier);

    libpostal_teardown(&instance);
    libpostal_teardown_language_classifier(&classifier);

}

