#include <stdio.h>
#include <stdlib.h>

#include "address_dictionary.h"

#include "address_expansion_data.c"
#include "collections.h"
#include "config.h"
#include "constants.h"
#include "gazetteer_data.c"

int main(int argc, char **argv) {
    char *output_file;
    if (argc > 1) {
        output_file = argv[1];
    } else {
        output_file = DEFAULT_ADDRESS_EXPANSION_PATH;
    }

    if (!address_dictionary_init()) {
        log_error("Error initializing address dictionary\n");
        exit(EXIT_FAILURE);
    }

    address_dictionary_t *address_dict = get_address_dictionary();

    khash_t(int_uint32) *dictionary_components = kh_init(int_uint32);

    khash_t(str_uint32) *canonical_indices = kh_init(str_uint32);

    khiter_t k;

    for (int g = 0; g < NUM_DICTIONARY_TYPES; g++) {
        gazetteer_t gazetteer = gazetteer_config[g];
        int ret;
        k = kh_put(int_uint32, dictionary_components, (uint32_t)gazetteer.type, &ret);
        kh_value(dictionary_components, k) = (uint32_t) gazetteer.address_components;
    }

    char_array *key = char_array_new();

    for (int i = 0; i < sizeof(expansion_languages) / sizeof(address_language_index_t); i++) {
        address_language_index_t lang_index = expansion_languages[i];
        char *language = lang_index.language;

        log_info("Doing language: %s\n", language);

        for (int j = lang_index.index; j < lang_index.index + lang_index.len; j++) {
            address_expansion_rule_t expansion_rule = expansion_rules[j];

            uint16_t address_components = 0;

            address_expansion_t expansion;
            expansion.separable = 0;

            strcpy(expansion.language, language);
            expansion.num_dictionaries = expansion_rule.num_dictionaries;

            for (int d = 0; d < expansion_rule.num_dictionaries; d++) {
                uint16_t dictionary_id = (uint16_t) expansion_rule.dictionaries[d];

                expansion.dictionary_ids[d] = dictionary_id;

                if (dictionary_id == DICTIONARY_CONCATENATED_PREFIX_SEPARABLE ||
                    dictionary_id == DICTIONARY_CONCATENATED_SUFFIX_SEPARABLE ||
                    dictionary_id == DICTIONARY_ELISION) {
                    expansion.separable = 1;
                }

                k = kh_get(int_uint32, dictionary_components, (uint32_t)dictionary_id);
                if (k == kh_end(dictionary_components)) {
                    log_error("Invalid dictionary_type: %d\n", dictionary_id);
                    exit(EXIT_FAILURE);
                }
                address_components |= (uint16_t) kh_value(dictionary_components, k);
            }

            expansion.address_components = address_components;

            char *canonical = NULL;
            if (expansion_rule.canonical_index != -1) {
                canonical = canonical_strings[expansion_rule.canonical_index];
            }

            if (canonical == NULL) {
                expansion.canonical_index = -1;
            } else {
                k = kh_get(str_uint32, canonical_indices, canonical);
                if (k != kh_end(canonical_indices)) {
                    expansion.canonical_index = kh_value(canonical_indices, k);
                } else {
                    expansion.canonical_index = address_dictionary_next_canonical_index();
                    if (!address_dictionary_add_canonical(canonical)) {
                        log_error("Error adding canonical string: %s\n", canonical);
                        exit(EXIT_FAILURE);
                    }
                }

            }

            // Add the phrase itself to the base namespace for existence checks

            if (!address_dictionary_add_expansion(expansion_rule.phrase, NULL, expansion)) {
                log_error("Could not add expansion {%s}\n", expansion_rule.phrase);
                exit(EXIT_FAILURE);
            }

            // Add phrase namespaced by language for language-specific matching

            if (!address_dictionary_add_expansion(expansion_rule.phrase, language, expansion)) {
                log_error("Could not add language expansion {%s, %s}\n", language, expansion_rule.phrase);
                exit(EXIT_FAILURE);
            }
        }
    }

    address_dictionary_save(output_file);

    char_array_destroy(key);

    kh_destroy(int_uint32, dictionary_components);

    kh_destroy(str_uint32, canonical_indices);

    address_dictionary_module_teardown();
}
