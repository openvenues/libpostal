#include <stdio.h>
#include <stdlib.h>

#include "address_dictionary.h"

#include "address_expansion_data.c"
#include "config.h"
#include "collections.h"
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

            if (!address_dictionary_add_expansion(address_dict, expansion_rule.phrase, canonical, language, dictionary_id, address_components)) {
                log_error("Could not add expansion {%s, %s}\n", language, expansion_rule.phrase);
                exit(EXIT_FAILURE);
            }

        }
    }


    address_dictionary_save(output_file);

    kh_destroy(int_int, dictionary_components);

    address_dictionary_module_teardown();
}