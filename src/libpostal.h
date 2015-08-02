#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "string_utils.h"

typedef struct normalize_options {
    // List of language codes
    int num_languages;
    char *languages[MAX_LANGUAGE_LEN];  
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

cstring_array *expand_address(char *input, normalize_options_t options);

bool libpostal_setup(void);
void libpostal_teardown(void);

#endif
