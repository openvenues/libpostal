/* normalize.h

The normalize module provides several options for preprocessing full strings:

- Unicode normalization (NFD/decomposition)
- Transliteration (including Latin-ASCII)
- Accent mark removal
- UTF-8 lowercasing with utf8proc

As well as normalizations for individual string tokens:

- Replace hyphens with space e.g. "quatre-vingt" => "quatre vingt"
- Delete hyphens e.g. "auto-estrada" => "autoestrada"
- Delete final period "R." => "R"
- Delete acronym periods: "U.S.A." => "USA"
- Drop English possessive "Janelle's" => "Janelle"
- Delete other apostrophes "O'Malley" => "OMalley" (not appropriate for Latin languages, use elision separation)

*/

#ifndef NORMALIZE_H
#define NORMALIZE_H

 

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "constants.h"
#include "klib/khash.h"
#include "libpostal.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"
#include "unicode_scripts.h"
#include "numex.h"
#include "scanner.h"
#include "transliterate.h"
#include "trie.h"
#include "tokens.h"
#include "vector.h"

#define NORMALIZE_STRING_LATIN_ASCII LIBPOSTAL_NORMALIZE_STRING_LATIN_ASCII
#define NORMALIZE_STRING_TRANSLITERATE LIBPOSTAL_NORMALIZE_STRING_TRANSLITERATE
#define NORMALIZE_STRING_STRIP_ACCENTS LIBPOSTAL_NORMALIZE_STRING_STRIP_ACCENTS
#define NORMALIZE_STRING_DECOMPOSE LIBPOSTAL_NORMALIZE_STRING_DECOMPOSE
#define NORMALIZE_STRING_LOWERCASE LIBPOSTAL_NORMALIZE_STRING_LOWERCASE
#define NORMALIZE_STRING_TRIM LIBPOSTAL_NORMALIZE_STRING_TRIM
#define NORMALIZE_STRING_REPLACE_HYPHENS LIBPOSTAL_NORMALIZE_STRING_REPLACE_HYPHENS
#define NORMALIZE_STRING_COMPOSE LIBPOSTAL_NORMALIZE_STRING_COMPOSE
#define NORMALIZE_STRING_SIMPLE_LATIN_ASCII LIBPOSTAL_NORMALIZE_STRING_SIMPLE_LATIN_ASCII
#define NORMALIZE_STRING_REPLACE_NUMEX LIBPOSTAL_NORMALIZE_STRING_REPLACE_NUMEX

#define NORMALIZE_TOKEN_REPLACE_HYPHENS LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_HYPHENS
#define NORMALIZE_TOKEN_DELETE_HYPHENS LIBPOSTAL_NORMALIZE_TOKEN_DELETE_HYPHENS
#define NORMALIZE_TOKEN_DELETE_FINAL_PERIOD LIBPOSTAL_NORMALIZE_TOKEN_DELETE_FINAL_PERIOD
#define NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS LIBPOSTAL_NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS
#define NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES LIBPOSTAL_NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES
#define NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE LIBPOSTAL_NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE
#define NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC LIBPOSTAL_NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC
#define NORMALIZE_TOKEN_REPLACE_DIGITS LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_DIGITS
#define NORMALIZE_TOKEN_REPLACE_NUMERIC_TOKEN_LETTERS LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_NUMERIC_TOKEN_LETTERS
#define NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS

// Replace digits with capital D e.g. 10013 => DDDDD, intended for use with lowercased strings
#define DIGIT_CHAR "D"

char *normalize_string_utf8(char *str, uint64_t options);

char *normalize_string_utf8_languages(char *str, uint64_t options, size_t num_languages, char **languages);
char *normalize_string_latin(char *str, size_t len, uint64_t options);
char *normalize_string_latin_languages(char *str, size_t len, uint64_t options, size_t num_languages, char **languages);


// Takes NORMALIZE_TOKEN_* options
void add_normalized_token(char_array *array, char *str, token_t token, uint64_t options);
void normalize_token(cstring_array *array, char *str, token_t token, uint64_t options);

bool numeric_starts_with_alpha(char *str, token_t token);

// Takes NORMALIZE_STRING_* options
string_tree_t *normalize_string(char *str, uint64_t options);
string_tree_t *normalize_string_languages(char *str, uint64_t options, size_t num_languages, char **languages);
 

#endif
