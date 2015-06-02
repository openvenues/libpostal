#ifndef NUMEX_RULE_H
#define NUMEX_RULE_H

#include <stdlib.h>
#include "numex.h"

typedef struct numex_rule_source {
    char *key;
    numex_rule_t rule;
} numex_rule_source_t;

typedef struct numex_language_source {
    char *name;
    size_t rule_index;
    size_t num_rules;
    size_t ordinal_indicator_index;
    size_t num_ordinal_indicators;
} numex_language_source_t;

#endif
