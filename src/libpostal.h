#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "export.h"

#define LIBPOSTAL_MAX_LANGUAGE_LEN 4

/* 
Address dictionaries
*/
// Bit set, should be able to keep it at a short (uint16_t)
#define LIBPOSTAL_ADDRESS_NONE 0
#define LIBPOSTAL_ADDRESS_ANY (1 << 0)
#define LIBPOSTAL_ADDRESS_NAME (1 << 1)
#define LIBPOSTAL_ADDRESS_HOUSE_NUMBER (1 << 2)
#define LIBPOSTAL_ADDRESS_STREET (1 << 3)
#define LIBPOSTAL_ADDRESS_UNIT (1 << 4)
#define LIBPOSTAL_ADDRESS_LEVEL (1 << 5)
#define LIBPOSTAL_ADDRESS_STAIRCASE (1 << 6)
#define LIBPOSTAL_ADDRESS_ENTRANCE (1 << 7)

#define LIBPOSTAL_ADDRESS_CATEGORY (1 << 8)
#define LIBPOSTAL_ADDRESS_NEAR (1 << 9)

#define LIBPOSTAL_ADDRESS_TOPONYM (1 << 13)
#define LIBPOSTAL_ADDRESS_POSTAL_CODE (1 << 14)
#define LIBPOSTAL_ADDRESS_PO_BOX (1 << 15)
#define LIBPOSTAL_ADDRESS_ALL ((1 << 16) - 1)

typedef struct libpostal_normalize_options {
    // List of language codes
    char **languages;  
    size_t num_languages;
    uint16_t address_components;

    // String options
    bool latin_ascii;
    bool transliterate;
    bool strip_accents;
    bool decompose;
    bool lowercase;
    bool trim_string;
    bool drop_parentheticals;
    bool replace_numeric_hyphens;
    bool delete_numeric_hyphens;
    bool split_alpha_from_numeric;
    bool replace_word_hyphens;
    bool delete_word_hyphens;
    bool delete_final_periods;
    bool delete_acronym_periods;
    bool drop_english_possessives;
    bool delete_apostrophes;
    bool expand_numex;
    bool roman_numerals;

} libpostal_normalize_options_t;

LIBPOSTAL_EXPORT libpostal_normalize_options_t libpostal_get_default_options(void);

LIBPOSTAL_EXPORT char **libpostal_expand_address(char *input, libpostal_normalize_options_t options, size_t *n);

LIBPOSTAL_EXPORT void libpostal_expansion_array_destroy(char **expansions, size_t n);

/*
Address parser
*/

typedef struct libpostal_address_parser_response {
    size_t num_components;
    char **components;
    char **labels;
} libpostal_address_parser_response_t;

typedef struct libpostal_address_parser_options {
    char *language;
    char *country;
} libpostal_address_parser_options_t;

LIBPOSTAL_EXPORT void libpostal_address_parser_response_destroy(libpostal_address_parser_response_t *self);

LIBPOSTAL_EXPORT libpostal_address_parser_options_t libpostal_get_address_parser_default_options(void);

LIBPOSTAL_EXPORT libpostal_address_parser_response_t *libpostal_parse_address(char *address, libpostal_address_parser_options_t options);

// Setup/teardown methods

LIBPOSTAL_EXPORT bool libpostal_setup(void);
LIBPOSTAL_EXPORT bool libpostal_setup_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown(void);

LIBPOSTAL_EXPORT bool libpostal_setup_parser(void);
LIBPOSTAL_EXPORT bool libpostal_setup_parser_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_parser(void);

LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier(void);
LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_language_classifier(void);

#ifdef __cplusplus
}
#endif

#endif
