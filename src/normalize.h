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
#include "string_utils.h"
#include "utf8proc/utf8proc.h"
#include "unicode_scripts.h"
#include "numex.h"
#include "transliterate.h"
#include "trie.h"
#include "tokens.h"
#include "vector.h"

#define NORMALIZE_STRING_LATIN_ASCII 1 << 0
#define NORMALIZE_STRING_TRANSLITERATE 1 << 1
#define NORMALIZE_STRING_STRIP_ACCENTS 1 << 2
#define NORMALIZE_STRING_DECOMPOSE 1 << 3
#define NORMALIZE_STRING_LOWERCASE 1 << 4
#define NORMALIZE_STRING_TRIM 1 << 5
#define NORMALIZE_STRING_REPLACE_HYPHENS 1 << 6
#define NORMALIZE_STRING_COMPOSE 1 << 7
#define NORMALIZE_STRING_SIMPLE_LATIN_ASCII 1 << 8
#define NORMALIZE_STRING_REPLACE_NUMEX 1 << 9

#define NORMALIZE_TOKEN_REPLACE_HYPHENS 1 << 0
#define NORMALIZE_TOKEN_DELETE_HYPHENS 1 << 1
#define NORMALIZE_TOKEN_DELETE_FINAL_PERIOD 1 << 2
#define NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS 1 << 3
#define NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES 1 << 4
#define NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE 1 << 5
#define NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC 1 << 6
#define NORMALIZE_TOKEN_REPLACE_DIGITS 1 << 7

// Replace digits with capital D e.g. 10013 => DDDDD, intended for use with lowercased strings
#define DIGIT_CHAR "D"
// Replace ideographic numbers with capital N e.g. ½ => N
#define IDEOGRAPHIC_NUMBER_CHAR "N"

char *normalize_string_utf8(char *str, uint64_t options);

char *normalize_string_latin(char *str, size_t len, uint64_t options);

// Takes NORMALIZE_TOKEN_* options
void add_normalized_token(char_array *array, char *str, token_t token, uint64_t options);
void normalize_token(cstring_array *array, char *str, token_t token, uint64_t options);

bool numeric_starts_with_alpha(char *str, token_t token);

// Takes NORMALIZE_STRING_* options
string_tree_t *normalize_string(char *str, uint64_t options);
string_tree_t *normalize_string_languages(char *str, uint64_t options, size_t num_languages, char **languages);
 

#endif
