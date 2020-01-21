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

#include "libpostal_types.h"
#include "constants.h"
#include "klib/khash.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"
#include "unicode_scripts.h"
#include "numex.h"
#include "scanner.h"
#include "transliterate.h"
#include "trie.h"
#include "tokens.h"
#include "vector.h"
#include "normalize_types.h"

#include "libpostal.h"

char *normalize_string_utf8(numex_table_t *numex_table, char *str, uint64_t options);

char *normalize_string_utf8_languages(numex_table_t *numex_table, char *str, uint64_t options, size_t num_languages, char **languages);
char *normalize_string_latin(libpostal_t *instance, char *str, size_t len, uint64_t options);
char *normalize_string_latin_languages(libpostal_t *instance, char *str, size_t len, uint64_t options, size_t num_languages, char **languages);


// Takes NORMALIZE_TOKEN_* options
void add_normalized_token(char_array *array, char *str, token_t token, uint64_t options);
void normalize_token(cstring_array *array, char *str, token_t token, uint64_t options);

bool numeric_starts_with_alpha(char *str, token_t token);

// Takes NORMALIZE_STRING_* options
string_tree_t *normalize_string(libpostal_t *instance, char *str, uint64_t options);
string_tree_t *normalize_string_languages(libpostal_t *instance, char *str, uint64_t options, size_t num_languages, char **languages);
 

#endif
