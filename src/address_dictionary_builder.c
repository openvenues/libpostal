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

    khash_t(int_int) *dictionary_components = kh_init(int_int);

    khiter_t k;

    for (int g = 0; g < NUM_DICTIONARY_TYPES; g++) {
        gazetteer_t gazetteer = gazetteer_config[g];
        int ret;
        k = kh_put(int_int, dictionary_components, (uint32_t)gazetteer.type, &ret);
        kh_value(dictionary_components, k) = (uint32_t) gazetteer.address_components;
    }

    char_array *key = char_array_new();

    for (int i = 0; i < sizeof(expansion_languages) / sizeof(address_language_index_t); i++) {
        address_language_index_t lang_index = expansion_languages[i];
        char *language = lang_index.language;

        for (int j = lang_index.index; j < lang_index.index + lang_index.len; j++) {
            address_expansion_rule_t expansion_rule = expansion_rules[j];
            uint16_t dictionary_id = (uint16_t) expansion_rule.dictionary;

            k = kh_get(int_int, dictionary_components, (uint32_t)dictionary_id);
            if (k == kh_end(dictionary_components)) {
                log_error("Invalid dictionary_type: %d\n", dictionary_id);
                exit(EXIT_FAILURE);
            }

            uint16_t address_components = (uint16_t) kh_value(dictionary_components, k);

            char *canonical = NULL;
            if (expansion_rule.canonical_index != -1) {
                canonical = canonical_strings[expansion_rule.canonical_index];
            }

            // Add the phrase itself to the base namespace for existence checks

            if (!address_dictionary_add_expansion(expansion_rule.phrase, canonical, language, dictionary_id, address_components)) {
                log_error("Could not add expansion {%s}\n", expansion_rule.phrase);
                exit(EXIT_FAILURE);
            }

            // Add phrase namespaced by language for language-specific matching

            char_array_clear(key);
            char_array_cat(key, language);
            char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);
            char_array_cat(key, expansion_rule.phrase);
            char *token = char_array_get_string(key);

            if (!address_dictionary_add_expansion(token, canonical, language, dictionary_id, address_components)) {
                log_error("Could not add language expansion {%s, %s}\n", language, expansion_rule.phrase);
                exit(EXIT_FAILURE);
            }
        }
    }

    address_dictionary_save(output_file);

    char_array_destroy(key);

    kh_destroy(int_int, dictionary_components);

    address_dictionary_module_teardown();
}