
#ifndef ADDRESS_EXPANSION_RULE_H
#define ADDRESS_EXPANSION_RULE_H

#include <stdlib.h>
#include <stdint.h>

#include "constants.h"
#include "gazetteers.h"

#define MAX_DICTIONARY_TYPES 5

typedef struct address_expansion_rule {
    char *phrase;
    uint32_t num_dictionaries;
    dictionary_type_t dictionaries[MAX_DICTIONARY_TYPES];
    int32_t canonical_index;
} address_expansion_rule_t;

typedef struct address_language_index {
    char language[MAX_LANGUAGE_LEN];
    uint32_t index;
    size_t len;
} address_language_index_t;


#endif
