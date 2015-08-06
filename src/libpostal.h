#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_LANGUAGE_LEN 4

// Bit set, should be able to keep it at a short (uint16_t)
#define ADDRESS_ANY 1 << 0
#define ADDRESS_NAME 1 << 1
#define ADDRESS_HOUSE_NUMBER 1 << 2
#define ADDRESS_STREET 1 << 3
#define ADDRESS_UNIT 1 << 4

#define ADDRESS_LOCALITY 1 << 7
#define ADDRESS_ADMIN1 1 << 8
#define ADDRESS_ADMIN2 1 << 9
#define ADDRESS_ADMIN3 1 << 10
#define ADDRESS_ADMIN4 1 << 11
#define ADDRESS_ADMIN_OTHER 1 << 12
#define ADDRESS_COUNTRY 1 << 13
#define ADDRESS_POSTAL_CODE 1 << 14
#define ADDRESS_NEIGHBORHOOD 1 << 15

typedef struct normalize_options {
    // List of language codes
    int num_languages;
    char **languages;  
    uint16_t address_components;

    // String options
    uint64_t latin_ascii:1;
    uint64_t transliterate:1;
    uint64_t strip_accents:1;
    uint64_t decompose:1;
    uint64_t lowercase:1;
    uint64_t trim_string:1;
    uint64_t drop_parentheticals:1;
    uint64_t replace_numeric_hyphens:1;
    uint64_t delete_numeric_hyphens:1;
    uint64_t split_alpha_from_numeric:1;
    uint64_t replace_word_hyphens:1;
    uint64_t delete_word_hyphens:1;
    uint64_t delete_final_periods:1;
    uint64_t delete_acronym_periods:1;
    uint64_t drop_english_possessives:1;
    uint64_t delete_apostrophes:1;
    uint64_t expand_numex:1;
    uint64_t roman_numerals:1;

} normalize_options_t;

char **expand_address(char *input, normalize_options_t options, uint64_t *n);

bool libpostal_setup(void);
void libpostal_teardown(void);

#endif
