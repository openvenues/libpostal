#include <dirent.h>
#include <limits.h>
#include <stdarg.h>

#include "address_dictionary.h"

#define ADDRESS_DICTIONARY_SIGNATURE 0xBABABABA

#define ADDRESS_DICTIONARY_SETUP_ERROR "address_dictionary module not setup, call libpostal_setup() or address_dictionary_module_setup()\n"

address_dictionary_t *address_dict = NULL;

address_dictionary_t *get_address_dictionary(void) {
    return address_dict;
}

address_expansion_value_t *address_dictionary_get_expansions(uint32_t i) {
    if (address_dict == NULL || address_dict->values == NULL || i > address_dict->values->n) {
        log_error("i=%" PRIu32 ", address_dict->values->n=%zu\n", i, address_dict->values->n);
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return NULL;
    }

    return address_dict->values->a[i];

}

inline bool address_expansion_in_dictionary(address_expansion_t expansion, uint16_t dictionary_id) {
    for (uint32_t i = 0; i < expansion.num_dictionaries; i++) {
        if (expansion.dictionary_ids[i] == dictionary_id) {
             return true;
        }
    }

    return false;
}


bool address_phrase_in_dictionary(phrase_t phrase, uint16_t dictionary_id) {
    address_expansion_value_t *value = address_dictionary_get_expansions(phrase.data);
    if (value == NULL) return false;

    address_expansion_array *expansions = value->expansions;
    if (expansions == NULL) return false;

    address_expansion_t *expansions_array = expansions->a;

    for (size_t i = 0; i < expansions->n; i++) {
        address_expansion_t expansion = expansions_array[i];
        if (address_expansion_in_dictionary(expansion, dictionary_id)) {
            return true;
        }
    }
    return false;
}


bool address_phrase_in_dictionaries(phrase_t phrase, size_t n, ...) {
    va_list args;
    va_start(args, n);
    bool in_dictionary = false;
    for (size_t i = 0; i < n; i++) {
        uint16_t dictionary_id = va_arg(args, uint16_t);
        in_dictionary = address_phrase_in_dictionary(phrase, dictionary_id);
        if (in_dictionary) break;
    }
    va_end(args);
    return in_dictionary;
}


int32_t address_dictionary_next_canonical_index(void) {
    if (address_dict == NULL || address_dict->canonical == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return -1;
    }
    return (int32_t)cstring_array_num_strings(address_dict->canonical);
}

bool address_dictionary_add_canonical(char *canonical) {
    if (address_dict == NULL || address_dict->canonical == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return false;
    }
    cstring_array_add_string(address_dict->canonical, canonical);
    return true;
}

char *address_dictionary_get_canonical(uint32_t index) {
    if (address_dict == NULL || address_dict->canonical == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return NULL;
    } else if (index > cstring_array_num_strings(address_dict->canonical)) {
        return NULL;
    } 
    return cstring_array_get_string(address_dict->canonical, index);    
}

inline bool address_expansions_have_canonical_interpretation(address_expansion_array *expansions) {
    if (expansions == NULL) return false;

    address_expansion_t *expansions_array = expansions->a;

    for (size_t i = 0; i < expansions->n; i++) {
        address_expansion_t expansion = expansions_array[i];
        if (expansion.canonical_index == NULL_CANONICAL_INDEX) {
            return true;
        }
    }
    return false;

}

inline bool address_phrase_has_canonical_interpretation(phrase_t phrase) {
    address_expansion_value_t *value = address_dictionary_get_expansions(phrase.data);
    if (value == NULL) return false;

    address_expansion_array *expansions = value->expansions;

    return address_expansions_have_canonical_interpretation(expansions);
}



address_expansion_value_t *address_expansion_value_new(void) {
    address_expansion_value_t *self = malloc(sizeof(address_expansion_value_t));

    if (self == NULL) return NULL;

    address_expansion_array *expansions = address_expansion_array_new();
    if (expansions == NULL) {
        free(self);
        return NULL;
    }

    self->components = 0;
    self->expansions = expansions;

    return self;
}

address_expansion_value_t *address_expansion_value_new_with_expansion(address_expansion_t expansion) {
    address_expansion_value_t *self = address_expansion_value_new();
    if (self == NULL) return NULL;

    address_expansion_array_push(self->expansions, expansion);
    self->components = expansion.address_components;

    return self;
}

void address_expansion_value_destroy(address_expansion_value_t *self) {
    if (self == NULL) return;
    if (self->expansions != NULL) {
        address_expansion_array_destroy(self->expansions);
    }

    free(self);
}

bool address_dictionary_add_expansion(char *name, char *language, address_expansion_t expansion) {
    if (address_dict == NULL || address_dict->values == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return false;
    }

    if (name == NULL) return false;

    char *key;

    bool is_prefix = false;
    bool is_suffix = false;
    bool is_phrase = false;

    for (size_t i = 0; i < expansion.num_dictionaries; i++) {
        dictionary_type_t dict = expansion.dictionary_ids[i];
        if (dict == DICTIONARY_CONCATENATED_SUFFIX_SEPARABLE || 
            dict == DICTIONARY_CONCATENATED_SUFFIX_INSEPARABLE) {
            is_suffix = true;
        } else if (dict == DICTIONARY_CONCATENATED_PREFIX_SEPARABLE ||
                   dict == DICTIONARY_ELISION) {
            is_prefix = true;
        } else {
            is_phrase = true;
        }
    }

    char_array *array = char_array_new_size(strlen(name));
    if (array == NULL) {
        return false;    
    }

    if (language != NULL) {
        char_array_cat(array, language);
        char_array_cat(array, NAMESPACE_SEPARATOR_CHAR);
    }

    if (!is_suffix && !is_prefix) {
        char_array_cat(array, name);
    } else if (is_prefix) {
        char_array_cat(array, TRIE_PREFIX_CHAR);
        char_array_cat(array, name);
    } else if (is_suffix) {
        char_array_cat(array, TRIE_SUFFIX_CHAR);
        char_array_cat_reversed(array, name);
    }

    key = char_array_to_string(array);

    log_debug("key=%s\n", key);

    uint32_t expansion_index;
    address_expansion_value_t *value;

    if (trie_get_data(address_dict->trie, key, &expansion_index)) {
        value = address_dict->values->a[expansion_index];
        value->components |= expansion.address_components;
        address_expansion_array_push(value->expansions, expansion);
    } else {
        value = address_expansion_value_new_with_expansion(expansion);
        expansion_index = (uint32_t)address_dict->values->n;
        address_expansion_value_array_push(address_dict->values, value);

        if (!trie_add(address_dict->trie, key, expansion_index)) {
            log_warn("Key %s could not be added to trie\n", key);
            goto exit_key_created;;
        }
    }

    free(key);

    return true;

exit_key_created:
    free(key);
    return false;
}

static trie_prefix_result_t get_language_prefix(char *lang) {
    if (lang == NULL) {
        return ROOT_PREFIX_RESULT;
    }

    trie_prefix_result_t prefix = trie_get_prefix(address_dict->trie, lang);

    if (prefix.node_id == NULL_NODE_ID) {
        return NULL_PREFIX_RESULT;
    }

    prefix = trie_get_prefix_from_index(address_dict->trie, NAMESPACE_SEPARATOR_CHAR, NAMESPACE_SEPARATOR_CHAR_LEN, prefix.node_id, prefix.tail_pos);

    if (prefix.node_id == NULL_NODE_ID) {
        return NULL_PREFIX_RESULT;
    }

    return prefix;
}

bool search_address_dictionaries_with_phrases(char *str, char *lang, phrase_array **phrases) {
    if (str == NULL) return false;
    if (address_dict == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return false;
    }

    trie_prefix_result_t prefix = get_language_prefix(lang);

    if (prefix.node_id == NULL_NODE_ID) {
        return false;
    }

    return trie_search_from_index(address_dict->trie, str, prefix.node_id, phrases);
}

phrase_array *search_address_dictionaries(char *str, char *lang) {
    phrase_array *phrases = NULL;

    if (!search_address_dictionaries_with_phrases(str, lang, &phrases)) {
        return NULL;
    }   

    return phrases;
}


bool search_address_dictionaries_tokens_with_phrases(char *str, token_array *tokens, char *lang, phrase_array **phrases) {
    if (str == NULL) return false;
    if (address_dict == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return false;
    }

    trie_prefix_result_t prefix = get_language_prefix(lang);

    if (prefix.node_id == NULL_NODE_ID) {
        return false;
    }

    return trie_search_tokens_from_index(address_dict->trie, str, tokens, prefix.node_id, phrases);
}


phrase_array *search_address_dictionaries_tokens(char *str, token_array *tokens, char *lang) {
    phrase_array *phrases = NULL;

    if (!search_address_dictionaries_tokens_with_phrases(str, tokens, lang, &phrases)) {
        return NULL;
    }

    return phrases;
}


phrase_t search_address_dictionaries_substring(char *str, size_t len, char *lang) {
    if (str == NULL) return NULL_PHRASE;
    if (address_dict == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return NULL_PHRASE;
    }

    trie_prefix_result_t prefix = get_language_prefix(lang);

    if (prefix.node_id == NULL_NODE_ID) {
        log_debug("prefix.node_id == NULL_NODE_ID\n");
        return NULL_PHRASE;
    }

    phrase_t phrase = trie_search_prefixes_from_index(address_dict->trie, str, len, prefix.node_id);
    if (phrase.len == len) {
        return phrase;
    } else {
        return NULL_PHRASE;
    }

}


phrase_t search_address_dictionaries_prefix(char *str, size_t len, char *lang) {
    if (str == NULL) return NULL_PHRASE;
    if (address_dict == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return NULL_PHRASE;
    }

    trie_prefix_result_t prefix = get_language_prefix(lang);

    if (prefix.node_id == NULL_NODE_ID) {
        log_debug("prefix.node_id == NULL_NODE_ID\n");
        return NULL_PHRASE;
    }

    return trie_search_prefixes_from_index_get_prefix_char(address_dict->trie, str, len, prefix.node_id);
}

phrase_t search_address_dictionaries_suffix(char *str, size_t len, char *lang) {
    if (str == NULL) return NULL_PHRASE;
    if (address_dict == NULL) {
        log_error(ADDRESS_DICTIONARY_SETUP_ERROR);
        return NULL_PHRASE;
    }

    trie_prefix_result_t prefix = get_language_prefix(lang);

    if (prefix.node_id == NULL_NODE_ID) {
        log_debug("prefix.node_id == NULL_NODE_ID\n");
        return NULL_PHRASE;
    }

    return trie_search_suffixes_from_index_get_suffix_char(address_dict->trie, str, len, prefix.node_id);
}

bool address_dictionary_init(void) {
    if (address_dict != NULL) return false;

    address_dict = calloc(1, sizeof(address_dictionary_t));
    if (address_dict == NULL) return false;

    address_dict->canonical = cstring_array_new();

    if (address_dict->canonical == NULL) {
        goto exit_destroy_address_dict;
    }

    address_dict->values = address_expansion_value_array_new();
    if (address_dict->values == NULL) {
        goto exit_destroy_address_dict;
    }

    address_dict->trie = trie_new();
    if (address_dict->trie == NULL) {
        goto exit_destroy_address_dict;
    }

    return true;

exit_destroy_address_dict:
    address_dictionary_destroy(address_dict);
    address_dict = NULL;
    return false;
}

void address_dictionary_destroy(address_dictionary_t *self) {
    if (self == NULL) return;

    if (self->canonical != NULL) {
        cstring_array_destroy(self->canonical);
    }

    if (self->values != NULL) {
        address_expansion_value_array_destroy(self->values);
    }

    if (self->trie != NULL) {
        trie_destroy(self->trie);
    }

    free(self);
}

static bool address_expansion_read(FILE *f, address_expansion_t *expansion) {
    if (f == NULL) return false;


    if (!file_read_uint32(f, (uint32_t *)&expansion->canonical_index)) {
        return false;
    }

    uint32_t language_len;

    if (!file_read_uint32(f, &language_len)) {
        return false;
    }

    if (!file_read_chars(f, expansion->language, language_len)) {
        return false;
    }

    if (!file_read_uint32(f, (uint32_t *)&expansion->num_dictionaries)) {
        return false;
    }

    for (size_t i = 0; i < expansion->num_dictionaries; i++) {
        if (!file_read_uint16(f, (uint16_t *)expansion->dictionary_ids + i)) {
            return false;
        }
    }

    if (!file_read_uint32(f, &expansion->address_components)) {
        return false;
    }

    if (!file_read_uint8(f, (uint8_t *)&expansion->separable)) {
        return false;
    }

    return true;
}

static address_expansion_value_t *address_expansion_value_read(FILE *f) {
    if (f == NULL) return NULL;

    address_expansion_value_t *value = address_expansion_value_new();

    if (!file_read_uint32(f, &value->components)) {
        goto exit_expansion_value_created;
    }

    uint32_t num_expansions;

    if (!file_read_uint32(f, &num_expansions)) {
        goto exit_expansion_value_created;
    }

    address_expansion_t expansion;

    for (size_t i = 0; i < num_expansions; i++) {
        if (!address_expansion_read(f, &expansion)) {
            goto exit_expansion_value_created;
        }
        address_expansion_array_push(value->expansions, expansion);
    }

    return value;

exit_expansion_value_created:
    address_expansion_value_destroy(value);
    return NULL;
}


static bool address_expansion_write(address_expansion_t expansion, FILE *f) {
    if (f == NULL) return false;

    uint32_t language_len = (uint32_t)strlen(expansion.language) + 1;

    if (!file_write_uint32(f, (uint32_t)expansion.canonical_index) ||
        !file_write_uint32(f, language_len) ||
        !file_write_chars(f, expansion.language, language_len) ||
        !file_write_uint32(f, expansion.num_dictionaries)
       ) {
        return false;
    }

    for (size_t i = 0; i < expansion.num_dictionaries; i++) {
        if (!file_write_uint16(f, expansion.dictionary_ids[i])) {
            return false;
        }
    }

    if (!file_write_uint32(f, expansion.address_components)) {
        return false;
    }

    if (!file_write_uint8(f, expansion.separable)) {
        return false;
    }

    return true;
}

static bool address_expansion_value_write(address_expansion_value_t *value, FILE *f) {
    if (value == NULL || value->expansions == NULL || f == NULL) return false;
    if (!file_write_uint32(f, value->components)) {
        return false;
    }

    uint32_t num_expansions = value->expansions->n;

    if (!file_write_uint32(f, num_expansions)) {
        return false;
    }

    for (size_t i = 0; i < num_expansions; i++) {
        address_expansion_t expansion = value->expansions->a[i];
        if (!address_expansion_write(expansion, f)) {
            return false;
        }
    }

    return true;
}


bool address_dictionary_write(FILE *f) {
    if (address_dict == NULL || f == NULL) return false;

    if (!file_write_uint32(f, ADDRESS_DICTIONARY_SIGNATURE)) {
        return false;
    }

    uint32_t canonical_str_len = (uint32_t) cstring_array_used(address_dict->canonical);
    if (!file_write_uint32(f, canonical_str_len)) {
        return false;
    }

    if (!file_write_chars(f, address_dict->canonical->str->a, canonical_str_len)) {
        return false;
    }

    uint32_t num_values = (uint32_t) address_dict->values->n;

    if (!file_write_uint32(f, num_values)) {
        return false;
    }

    for (size_t i = 0; i < num_values; i++) {
        address_expansion_value_t *value = address_dict->values->a[i];
        if (!address_expansion_value_write(value, f)) {
            return false;
        }
    }

    if (!trie_write(address_dict->trie, f)) {
        return false;
    }

    return true;
}

bool address_dictionary_read(FILE *f) {
    if (address_dict != NULL) return false;

    uint32_t signature;

    if (!file_read_uint32(f, &signature) || signature != ADDRESS_DICTIONARY_SIGNATURE) {
        return false;
    }

    address_dict = malloc(sizeof(address_dictionary_t));
    if (address_dict == NULL) return false;

    uint32_t canonical_str_len;

    if (!file_read_uint32(f, &canonical_str_len)) {
        goto exit_address_dict_created;
    }

    char_array *array = char_array_new_size(canonical_str_len);

    if (array == NULL) {
        goto exit_address_dict_created;
    }

    if (!file_read_chars(f, array->a, canonical_str_len)) {
        char_array_destroy(array);
        goto exit_address_dict_created;
    }

    array->n = canonical_str_len;

    address_dict->canonical = cstring_array_from_char_array(array);

    uint32_t num_values;

    if (!file_read_uint32(f, &num_values)) {
        goto exit_address_dict_created;
    }

    address_dict->values = address_expansion_value_array_new_size(num_values);

    for (uint32_t i = 0; i < num_values; i++) {
        address_expansion_value_t *value = address_expansion_value_read(f);
        if (value == NULL) {
            goto exit_address_dict_created;
        }
        address_expansion_value_array_push(address_dict->values, value);
    }

    address_dict->trie = trie_read(f);

    if (address_dict->trie == NULL) {
        goto exit_address_dict_created;
    }

    return true;

exit_address_dict_created:
    address_dictionary_destroy(address_dict);
    return false;
}


bool address_dictionary_load(char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }

    bool ret_val = address_dictionary_read(f);
    fclose(f);
    return ret_val;
}

bool address_dictionary_save(char *path) {
    if (address_dict == NULL) return false;

    FILE *f = fopen(path, "wb");

    bool ret_val = address_dictionary_write(f);
    fclose(f);
    return ret_val;
}

inline bool address_dictionary_module_setup(char *filename) {
    if (address_dict == NULL) {
        return address_dictionary_load(filename == NULL ? DEFAULT_ADDRESS_EXPANSION_PATH: filename);
    }

    return true;
}

void address_dictionary_module_teardown(void) {
    if (address_dict != NULL) {
        address_dictionary_destroy(address_dict);
    }
    address_dict = NULL;
}
