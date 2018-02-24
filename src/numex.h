#ifndef NUMEX_H
#define NUMEX_H


 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "collections.h"
#include "libpostal_config.h"
#include "constants.h"
#include "klib/khash.h"
#include "string_utils.h"
#include "tokens.h"
#include "trie.h"
#include "trie_search.h"

#define NUMEX_DATA_FILE "numex.dat"
#define DEFAULT_NUMEX_PATH LIBPOSTAL_DATA_DIR PATH_SEPARATOR "numex" PATH_SEPARATOR NUMEX_DATA_FILE

#define LATIN_LANGUAGE_CODE "la"

#define GENDER_MASCULINE_PREFIX "m"
#define GENDER_FEMININE_PREFIX "f"
#define GENDER_NEUTER_PREFIX "n"
#define GENDER_NONE_PREFIX "d"

typedef enum {
    GENDER_MASCULINE,
    GENDER_FEMININE,
    GENDER_NEUTER,
    GENDER_NONE,
    NUM_GENDERS
} gender_t;

#define CATEGORY_PLURAL_PREFIX "p"
#define CATEGORY_DEFAULT_PREFIX "s"

typedef enum {
    CATEGORY_PLURAL,
    CATEGORY_DEFAULT,
    NUM_CATEGORIES
} grammatical_category_t;

typedef enum {
    NUMEX_LEFT_CONTEXT_NONE,
    NUMEX_LEFT_CONTEXT_ADD,
    NUMEX_LEFT_CONTEXT_MULTIPLY,
    NUMEX_LEFT_CONTEXT_CONCAT_ONLY_IF_NUMBER
} numex_left_context;

typedef enum {
    NUMEX_RIGHT_CONTEXT_NONE,
    NUMEX_RIGHT_CONTEXT_ADD
} numex_right_context;

typedef enum {
    NUMEX_CARDINAL_RULE,
    NUMEX_ORDINAL_RULE,
    NUMEX_ORDINAL_INDICATOR_RULE,
    NUMEX_DECIMAL_RULE,
    NUMEX_NEGATION_RULE,
    NUMEX_STOPWORD,
    NUMEX_NULL
} numex_rule_type;

typedef struct numex_rule {
    numex_left_context left_context_type;
    numex_right_context right_context_type;
    numex_rule_type rule_type;
    gender_t gender;
    grammatical_category_t category;
    uint32_t radix;
    int64_t value;
} numex_rule_t;

#define NUMEX_NULL_RULE_INDEX 0
#define NUMEX_STOPWORD_INDEX 1

#define NUMEX_NULL_RULE (numex_rule_t) {NUMEX_LEFT_CONTEXT_NONE, NUMEX_RIGHT_CONTEXT_NONE, NUMEX_NULL, GENDER_NONE, CATEGORY_DEFAULT, 0, 0LL}
#define NUMEX_STOPWORD_RULE {NUMEX_LEFT_CONTEXT_NONE, NUMEX_RIGHT_CONTEXT_NONE, NUMEX_STOPWORD, GENDER_NONE, CATEGORY_DEFAULT, 0, 0LL}

VECTOR_INIT(numex_rule_array, numex_rule_t)

#define ORDINAL_NAMESPACE_CHAR "o"
#define ORDINAL_PHRASE_NAMESPACE_CHAR "p"

#define ORDINAL_NAMESPACE_PREFIX NAMESPACE_SEPARATOR_CHAR ORDINAL_NAMESPACE_CHAR NAMESPACE_SEPARATOR_CHAR            
#define ORDINAL_NAMESPACE_PREFIX_LEN strlen(ORDINAL_NAMESPACE_PREFIX)

#define ORDINAL_PHRASE_NAMESPACE_PREFIX NAMESPACE_SEPARATOR_CHAR ORDINAL_PHRASE_NAMESPACE_CHAR NAMESPACE_SEPARATOR_CHAR
#define ORDINAL_PHRASE_NAMESPACE_PREFIX_LEN strlen(ORDINAL_PHRASE_NAMESPACE_PREFIX)

typedef struct ordinal_indicator {
    char *key;
    gender_t gender;
    grammatical_category_t category;
    char *suffix;
} ordinal_indicator_t;


ordinal_indicator_t *ordinal_indicator_new(char *key, gender_t gender, grammatical_category_t category, char *suffix);
void ordinal_indicator_destroy(ordinal_indicator_t *self);

VECTOR_INIT_FREE_DATA(ordinal_indicator_array, ordinal_indicator_t *, ordinal_indicator_destroy)

typedef struct numex_language {
    char *name;
    bool whole_tokens_only;
    size_t rules_index;
    size_t num_rules;
    size_t ordinals_index;
    size_t num_ordinals;
} numex_language_t;

KHASH_MAP_INIT_STR(str_numex_language, numex_language_t *)

typedef struct {
    khash_t(str_numex_language) *languages;
    trie_t *trie;
    numex_rule_array *rules;
    ordinal_indicator_array *ordinal_indicators;
} numex_table_t;

numex_table_t *get_numex_table(void);

numex_language_t *numex_language_new(char *name, bool whole_tokens_only, size_t rules_index, size_t num_rules, size_t ordinals_index, size_t num_ordinals);
void numex_language_destroy(numex_language_t *self);

bool numex_table_add_language(numex_language_t *language);

numex_language_t *get_numex_language(char *name);

typedef struct numex_result {
    int64_t value;
    gender_t gender;
    grammatical_category_t category;
    bool is_ordinal;
    size_t start;
    size_t len;
} numex_result_t;

VECTOR_INIT(numex_result_array, numex_result_t)

char *replace_numeric_expressions(char *str, char *lang);
numex_result_array *convert_numeric_expressions(char *str, char *lang);
size_t ordinal_suffix_len(char *s, size_t len, char *lang);
size_t possible_ordinal_digit_len(char *str, size_t len);

size_t valid_ordinal_suffix_len(char *str, token_t token, token_t prev_token, char *lang);
bool add_ordinal_suffix_lengths(uint32_array *suffixes, char *str, token_array *tokens_array, char *lang);

bool is_likely_roman_numeral(char *str);
bool is_likely_roman_numeral_len(char *str, size_t len);

bool numex_table_write(FILE *file);
bool numex_table_save(char *filename);

bool numex_module_init(void);
bool numex_module_setup(char *filename);
void numex_module_teardown(void);

 

#endif
