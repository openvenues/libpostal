#ifndef ADDRESS_DICTIONARY_H
#define ADDRESS_DICTIONARY_H

 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <stdbool.h>
#include <string.h>

#include "address_expansion_rule.h"
#include "libpostal_config.h"
#include "constants.h"
#include "collections.h"
#include "file_utils.h"
#include "gazetteers.h"
#include "trie.h"
#include "trie_search.h"

#define ALL_LANGUAGES "all"

#define ADDRESS_DICTIONARY_DATA_FILE "address_dictionary.dat"
#define DEFAULT_ADDRESS_EXPANSION_PATH LIBPOSTAL_DATA_DIR PATH_SEPARATOR LIBPOSTAL_ADDRESS_EXPANSIONS_SUBDIR PATH_SEPARATOR ADDRESS_DICTIONARY_DATA_FILE

#define NULL_CANONICAL_INDEX -1

typedef struct address_expansion {
    int32_t canonical_index;
    char language[MAX_LANGUAGE_LEN];
    uint32_t num_dictionaries;
    uint16_t dictionary_ids[MAX_DICTIONARY_TYPES];
    uint32_t address_components;
    bool separable;
} address_expansion_t;

VECTOR_INIT(address_expansion_array, address_expansion_t)

typedef struct address_expansion_value {
    uint32_t components;
    address_expansion_array *expansions;
} address_expansion_value_t;

address_expansion_value_t *address_expansion_value_new(void);
address_expansion_value_t *address_expansion_value_new_with_expansion(address_expansion_t expansion);
void address_expansion_value_destroy(address_expansion_value_t *self);

VECTOR_INIT_FREE_DATA(address_expansion_value_array, address_expansion_value_t *, address_expansion_value_destroy)

typedef struct address_dictionary {
    cstring_array *canonical;
    address_expansion_value_array *values;
    trie_t *trie;
} address_dictionary_t;

#include "libpostal.h"

address_dictionary_t *address_dictionary_init(void);

phrase_array *search_address_dictionaries(address_dictionary_t *address_dict, char *str, char *lang);
bool search_address_dictionaries_with_phrases(address_dictionary_t *address_dict, char *str, char *lang, phrase_array **phrases);
phrase_array *search_address_dictionaries_tokens(address_dictionary_t *address_dict, char *str, token_array *tokens, char *lang);
bool search_address_dictionaries_tokens_with_phrases(address_dictionary_t *address_dict, char *str, token_array *tokens, char *lang, phrase_array **phrases);

phrase_t search_address_dictionaries_substring(address_dictionary_t *address_dict, char *str, size_t len, char *lang);
phrase_t search_address_dictionaries_prefix(address_dictionary_t *address_dict, char *str, size_t len, char *lang);
phrase_t search_address_dictionaries_suffix(address_dictionary_t *address_dict, char *str, size_t len, char *lang);

address_expansion_value_t *address_dictionary_get_expansions(address_dictionary_t *address_dict, uint32_t i);
bool address_expansion_in_dictionary(address_expansion_t expansion, uint16_t dictionary_id);
bool address_phrase_in_dictionary(address_dictionary_t *address_dict, phrase_t phrase, uint16_t dictionary_id);
bool address_phrase_in_dictionaries(address_dictionary_t *address_dict, phrase_t phrase, size_t n, ...);
char *address_dictionary_get_canonical(address_dictionary_t *address_dict, uint32_t index);
int32_t address_dictionary_next_canonical_index(address_dictionary_t *address_dict);
bool address_dictionary_add_canonical(address_dictionary_t *address_dict, char *canonical);
bool address_dictionary_add_expansion(address_dictionary_t *address_dict, char *key, char *language, address_expansion_t expansion);
bool address_expansions_have_canonical_interpretation(address_expansion_array *expansions);
bool address_phrase_has_canonical_interpretation(address_dictionary_t *address_dict, phrase_t phrase);

void address_dictionary_destroy(address_dictionary_t *self);

address_dictionary_t *address_dictionary_load(char *path);
bool address_dictionary_save(address_dictionary_t *address_dict, char *path);

address_dictionary_t *address_dictionary_module_setup(char *filename);
void address_dictionary_module_teardown(address_dictionary_t **address_dict);

#endif
