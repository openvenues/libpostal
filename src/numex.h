#ifndef NUMEX_H
#define NUMEX_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "string_utils.h"
#include "tokens.h"
#include "collections.h"

#define DEFAULT_NUMEX_PATH LIBPOSTAL_DATA_DIR "/numex/numex.dat"

typedef enum {
    GENDER_MASCULINE,
    GENDER_FEMININE,
    GENDER_NEUTER,
    GENDER_NONE
} gender_t;

typedef enum {
    NUMEX_LEFT_CONTEXT_NONE,
    NUMEX_LEFT_CONTEXT_ADD,
    NUMEX_LEFT_CONTEXT_MULTIPLY
} numex_left_context;

typedef enum {
    NUMEX_RIGHT_CONTEXT_NONE,
    NUMEX_RIGHT_CONTEXT_ADD
} numex_right_context;

typedef enum {
    NUMEX_CARDINAL_RULE,
    NUMEX_ORDINAL_RULE,
    NUMEX_DECIMAL_RULE,
    NUMEX_NEGATION_RULE,
    NUMEX_STOPWORD
} numex_rule_type;

typedef struct numex_rule {
    numex_left_context_type left_context_type;
    numex_right_context_type right_context_type;
    numex_rule_type rule_type;
    gender_t gender;
    uint32_t radix;
    int64_t value;
} numex_rule_t;

#define NUMEX_STOPWORD_RULE (numex_rule_t) {NUMEX_LEFT_CONTEXT_NONE, NUMEX_RIGHT_CONTEXT_NONE, NUMEX_STOPWORD, GENDER_NONE, 0, 0};

VECTOR_INIT(numex_rule_array, numex_rule_t)

typedef struct ordinal_indicator {
    uint8_t number;
    gender_t gender;
    char *indicator;
} ordinal_indicator_t;

void ordinal_indicator_destroy(ordinal_indicator_t *self);

VECTOR_INIT_FREE_DATA(ordinal_indicator_array, ordinal_indicator_t *, ordinal_indicator_destroy)

typedef struct numex_language {
    char *name;
    bool concatenated;
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

numex_language_t *numex_language_new(char *name, bool concatenated, size_t rules_index, size_t num_rules, size_t ordinals_index, size_t num_ordinals)
void numex_language_destroy(numex_language_t *self);

bool numex_table_add_language(numex_language_t *language);

numex_language_t *get_numex_language(char *name);
char *convert_numeric_expressions(char *input, token_array *input);

bool numex_table_write(FILE *file);
bool numex_table_save(char *filename);

void numex_module_setup(char *filename);
void numex_module_teardown(void);

#ifdef __cplusplus
}
#endif

#endif