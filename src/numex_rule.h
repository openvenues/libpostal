#ifndef NUMEX_RULE_H
#define NUMEX_RULE_H

#include <stdlib.h>
#include "numex.h"

typedef struct numex_language_source {
    char *name;
    bool whole_tokens_only;
    size_t rule_index;
    size_t num_rules;
    size_t ordinal_indicator_index;
    size_t num_ordinal_indicators;
} numex_language_source_t;

#endif
