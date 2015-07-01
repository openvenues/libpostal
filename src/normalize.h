#ifndef NORMALIZE_H
#define NORMALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "constants.h"
#include "klib/khash.h"
#include "numex.h"
#include "scanner.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"
#include "unicode_scripts.h"
#include "transliterate.h"
#include "trie.h"
#include "tokens.h"
#include "vector.h"

#define NORMALIZE_STRING_LATIN_ASCII 1 << 0
#define NORMALIZE_STRING_TRANSLITERATE 1 << 1
#define NORMALIZE_STRING_STRIP_ACCENTS 1 << 2
#define NORMALIZE_STRING_DECOMPOSE 1 << 3
#define NORMALIZE_STRING_LOWERCASE 1 << 4

#define NORMALIZE_TOKEN_REPLACE_HYPHENS 1 << 0
#define NORMALIZE_TOKEN_DELETE_HYPHENS 1 << 1
#define NORMALIZE_TOKEN_DELETE_FINAL_PERIOD 1 << 2
#define NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS 1 << 3
#define NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES 1 << 4
#define NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE 1 << 5

char *utf8_normalize_string(char *str, uint64_t options);

// Takes NORMALIZE_TOKEN_* options
bool add_token_alternatives(cstring_array *array, char *str, token_t token, uint64_t options);

// Takes NORMALIZE_STRING_* options
string_tree_t *normalize_string(char *str, uint64_t options);

#ifdef __cplusplus
}
#endif

#endif