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

    int ret;

    for (int g = 0; g < NUM_DICTIONARY_TYPES; g++) {
        gazetteer_t gazetteer = gazetteer_config[g];
        k = kh_put(int_uint32, dictionary_components, (uint32_t)gazetteer.type, &ret);
        kh_value(dictionary_components, k) = (uint32_t) gazetteer.address_components;
    }

    char_array *key = char_array_new();

    size_t num_expansion_languages = sizeof(expansion_languages) / sizeof(address_language_index_t);

    log_info("num_expansion_languages = %zu\n", num_expansion_languages);

    khash_t(str_uint32) *phrase_address_components = kh_init(str_uint32);

    for (size_t i = 0; i < num_expansion_languages; i++) {
        address_language_index_t lang_index = expansion_languages[i];
        char *language = lang_index.language;

        kh_clear(str_uint32, phrase_address_components);

        log_info("Doing language: %s\n", language);
        for (int add_affixes = 0; add_affixes <= 1; add_affixes++) {

            for (int j = lang_index.index; j < lang_index.index + lang_index.len; j++) {
                address_expansion_rule_t expansion_rule = expansion_rules[j];

                uint32_t address_components = 0;

                address_expansion_t expansion;
                expansion.separable = 0;

                strcpy(expansion.language, language);
                expansion.num_dictionaries = expansion_rule.num_dictionaries;

                bool is_affix = false;

                for (int d = 0; d < expansion_rule.num_dictionaries; d++) {
                    uint16_t dictionary_id = (uint16_t) expansion_rule.dictionaries[d];

                    expansion.dictionary_ids[d] = dictionary_id;

                    if (dictionary_id == DICTIONARY_CONCATENATED_PREFIX_SEPARABLE ||
                        dictionary_id == DICTIONARY_CONCATENATED_SUFFIX_SEPARABLE ||
                        dictionary_id == DICTIONARY_ELISION) {
                        expansion.separable = 1;
                        is_affix = true;
                    } else if (dictionary_id == DICTIONARY_CONCATENATED_SUFFIX_INSEPARABLE) {
                        is_affix = true;
                    }

                    k = kh_get(int_uint32, dictionary_components, (uint32_t)dictionary_id);
                    if (k == kh_end(dictionary_components)) {
                        log_error("Invalid dictionary_type: %d\n", dictionary_id);
                        exit(EXIT_FAILURE);
                    }
                    address_components |= kh_value(dictionary_components, k);
                }

                char *canonical = NULL;
                if (expansion_rule.canonical_index != -1) {
                    canonical = canonical_strings[expansion_rule.canonical_index];
                }

                if (!add_affixes || !is_affix) {
                    expansion.address_components = address_components;
                } else {
                    char *phrase = canonical == NULL ? expansion_rule.phrase : canonical;

                    k = kh_get(str_uint32, phrase_address_components, phrase);

                    if (k != kh_end(phrase_address_components)) {
                        uint32_t val = kh_value(phrase_address_components, k);
                        expansion.address_components = val;
                    } else {
                        expansion.address_components = address_components;
                    }
                }

                if (canonical == NULL) {
                    expansion.canonical_index = -1;

                    if (!add_affixes && is_affix) {
                        uint32_t phrase_components = (uint32_t)address_components;

                        k = kh_get(str_uint32, phrase_address_components, expansion_rule.phrase);
                        if (k != kh_end(phrase_address_components)) {
                            phrase_components |= kh_value(phrase_address_components, k);
                        }

                        k = kh_put(str_uint32, phrase_address_components, expansion_rule.phrase, &ret);

                        if (ret < 0) {
                            log_error("Error in kh_put on phrase_address_components\n");
                            exit(EXIT_FAILURE);
                        }
                        kh_value(phrase_address_components, k) = phrase_components;
                    }

                } else {

                    if (add_affixes) {
                        k = kh_get(str_uint32, canonical_indices, canonical);
                        if (k != kh_end(canonical_indices)) {
                            expansion.canonical_index = kh_value(canonical_indices, k);
                        } else {
                            uint32_t canonical_index = address_dictionary_next_canonical_index();
                            if (!address_dictionary_add_canonical(canonical)) {
                                log_error("Error adding canonical string: %s\n", canonical);
                                exit(EXIT_FAILURE);
                            }
                            expansion.canonical_index = canonical_index;
                            k = kh_put(str_uint32, canonical_indices, canonical, &ret);
                            if (ret < 0) {
                                log_error("Error in kh_put on canonical_indices\n");
                                exit(EXIT_FAILURE);
                            }
                            kh_value(canonical_indices, k) = canonical_index;
                        }
                    }
                }

                if (add_affixes) {
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
        }
    }



    address_dictionary_save(output_file);

    char_array_destroy(key);

    kh_destroy(int_uint32, dictionary_components);

    kh_destroy(str_uint32, canonical_indices);
    kh_destroy(str_uint32, phrase_address_components);

    address_dictionary_module_teardown();
}
