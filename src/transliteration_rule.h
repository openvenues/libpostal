#ifndef TRANSLITERATION_RULE_H
#define TRANSLITERATION_RULE_H

#include <stdlib.h>

#define MAX_GROUPS_LEN 5

typedef enum {
    CONTEXT_TYPE_NONE,
    CONTEXT_TYPE_STRING,
    CONTEXT_TYPE_WORD_BOUNDARY,
    CONTEXT_TYPE_REGEX
} context_type_t;

typedef struct transliteration_rule_source {
    char *key;
    size_t key_len;

    context_type_t pre_context_type;
    size_t pre_context_max_len;
    char *pre_context;
    size_t pre_context_len;

    context_type_t post_context_type;
    size_t post_context_max_len;
    char *post_context;
    size_t post_context_len;

    char *replacement;
    size_t replacement_len;

    char *revisit;
    size_t revisit_len;

    char *group_regex_str;
    size_t group_regex_len;

} transliteration_rule_source_t;

typedef struct transliteration_step_source {
    step_type_t type;
    int rules_start;
    int rules_length;
    char *name;
} transliteration_step_source_t;

typedef struct transliterator_source {
    char *name;
    uint8_t internal;
    int steps_start;
    int steps_length;
} transliterator_source_t;


#endif
