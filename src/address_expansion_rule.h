#include <stdlib.h>

#include "constants.h"

typedef struct address_expansion_rule {
    char *phrase;
    dictionary_type_t dictionary;
    int32_t canonical_index;
} address_expansion_rule_t;

typedef struct address_language_index {
    char language[MAX_LANGUAGE_LEN];
    uint32_t index;
    size_t len;
} address_language_index_t;