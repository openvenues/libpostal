#ifndef ADDRESS_DICTIONARY_H
#define ADDRESS_DICTIONARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "address_expansion_rule.h"
#include "config.h"
#include "constants.h"
#include "collections.h"
#include "file_utils.h"
#include "gazetteers.h"
#include "trie.h"
#include "trie_search.h"

#define DEFAULT_ADDRESS_EXPANSION_PATH LIBPOSTAL_DATA_DIR PATH_SEPARATOR "address_expansions" PATH_SEPARATOR "address_dictionary.dat"

typedef union expansion_value {
    uint32_t value;
    struct {
        uint32_t components:16;
        uint32_t count:15;
        uint32_t canonical:1;
    };
} expansion_value_t;

typedef struct address_expansion {
    int32_t canonical_index;
    char language[MAX_LANGUAGE_LEN];
    uint16_t dictionary_id;
    uint16_t address_components;
} address_expansion_t;

VECTOR_INIT(address_expansion_array, address_expansion_t)

KHASH_MAP_INIT_STR(str_expansions, address_expansion_array *)

typedef struct address_dictionary {
    cstring_array *canonical;
    khash_t(str_expansions) *expansions;
    trie_t *trie;
} address_dictionary_t;

address_dictionary_t *get_address_dictionary(void);

bool address_dictionary_init(void);

phrase_array *search_address_dictionaries(char *str, char *lang);
address_expansion_array *address_dictionary_get_expansions(char *key);
char *address_dictionary_get_canonical(uint32_t index);
bool address_dictionary_add_expansion(char *key, char *canonical, char *language, uint16_t dictionary_id, uint16_t address_components);

void address_dictionary_destroy(address_dictionary_t *self);

bool address_dictionary_load(char *path);
bool address_dictionary_save(char *path);

bool address_dictionary_module_setup(void);
void address_dictionary_module_teardown(void);


#ifdef __cplusplus
}
#endif

#endif