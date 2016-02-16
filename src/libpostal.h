#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_LANGUAGE_LEN 4

/* 
Address dictionaries
*/
// Bit set, should be able to keep it at a short (uint16_t)
#define ADDRESS_NONE 0
#define ADDRESS_ANY (1 << 0)
#define ADDRESS_NAME (1 << 1)
#define ADDRESS_HOUSE_NUMBER (1 << 2)
#define ADDRESS_STREET (1 << 3)
#define ADDRESS_UNIT (1 << 4)

#define ADDRESS_LOCALITY (1 << 7)
#define ADDRESS_ADMIN1 (1 << 8)
#define ADDRESS_ADMIN2 (1 << 9)
#define ADDRESS_ADMIN3 (1 << 10)
#define ADDRESS_ADMIN4 (1 << 11)
#define ADDRESS_ADMIN_OTHER (1 << 12)
#define ADDRESS_COUNTRY (1 << 13)
#define ADDRESS_POSTAL_CODE (1 << 14)
#define ADDRESS_NEIGHBORHOOD (1 << 15)
#define ADDRESS_ALL ((1 << 16) - 1)

typedef struct normalize_options {
    // List of language codes
    char **languages;  
    int num_languages;
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

} normalize_options_t;

normalize_options_t get_libpostal_default_options(void);

char **expand_address(char *input, normalize_options_t options, size_t *n);

void expansion_array_destroy(char **expansions, size_t n);

/*
Address parser
*/

typedef struct address_parser_response {
    size_t num_components;
    char **components;
    char **labels;
} address_parser_response_t;

typedef struct address_parser_options {
    char *language;
    char *country;
} address_parser_options_t;

void address_parser_response_destroy(address_parser_response_t *self);

address_parser_options_t get_libpostal_address_parser_default_options(void);

address_parser_response_t *parse_address(char *address, address_parser_options_t options);

// Setup/teardown methods

bool libpostal_setup(void);
void libpostal_teardown(void);

bool libpostal_setup_parser(void);
void libpostal_teardown_parser(void);

bool libpostal_setup_language_classifier(void);
void libpostal_teardown_language_classifier(void);

#ifdef __cplusplus
}
#endif

#endif
