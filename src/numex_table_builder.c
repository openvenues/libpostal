#include <stdio.h>
#include <stdlib.h>


#include "constants.h"
#include "log/log.h"
#include "numex.h"
#include "numex_rule.h"
#include "numex_data.c"


int main(int argc, char **argv) {
    char *filename;

    if (argc == 2) {
        filename = argv[1];
    } else {
        filename = DEFAULT_NUMEX_PATH;
    }

    FILE *f = fopen(filename, "wb");

    if (f == NULL) {
        log_error("File could not be opened, ensure directory exists: %s\n", filename);
        numex_module_teardown();
        exit(1);
    }

    if (!numex_module_init()) {
        log_error("Numex table initialization unsuccessful\n");
        numex_module_teardown();
        exit(1);
    }

    numex_table_t *numex_table = get_numex_table();

    size_t num_languages = sizeof(numex_languages) / sizeof(numex_language_source_t);

    size_t num_source_keys = sizeof(numex_keys) / sizeof(char *);
    size_t num_source_rules = sizeof(numex_rules) / sizeof(numex_rule_t);

    if (num_source_keys != num_source_rules) {
        log_error("num_sourcE_keys != num_source_rules, aborting\n");
        numex_module_teardown();
        exit(1);
    }

    size_t num_ordinal_indicator_rules = sizeof(ordinal_indicator_rules) / sizeof(ordinal_indicator_t);

    char_array *key = char_array_new();

    for (int i = 0; i < num_languages; i++) {
        numex_language_source_t lang_source = numex_languages[i];

        char *lang = lang_source.name;

        int j;

        size_t rule_index = lang_source.rule_index;
        size_t num_rules = lang_source.num_rules;
        size_t ordinal_indicator_index = lang_source.ordinal_indicator_index;
        size_t num_ordinal_indicators = lang_source.num_ordinal_indicators;

        numex_rule_t rule;

        uint32_t value;

        log_info("Doing language=%s\n", lang);

        for (j = rule_index; j < rule_index + num_rules; j++) {
            char *numex_key = numex_keys[j];
            numex_rule_t rule = numex_rules[j];

            value = rule.rule_type != NUMEX_STOPWORD ? numex_table->rules->n : NUMEX_STOPWORD_INDEX;
            numex_rule_array_push(numex_table->rules, rule);

            char_array_clear(key);
            char_array_cat(key, lang);
            char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);
            char_array_cat(key, numex_key);

            char *str_key = char_array_get_string(key);

            trie_add(numex_table->trie, str_key, value);

            if (string_contains_hyphen(str_key)) {
                char *replaced = string_replace_char(str_key, '-', ' ');
                trie_add(numex_table->trie, replaced, value);
                free(replaced);
            }

        }

        for (j = ordinal_indicator_index; j < ordinal_indicator_index + num_ordinal_indicators; j++) {
            for (int ordinal_phrases = 0; ordinal_phrases <= 1; ordinal_phrases++) {
                value = numex_table->ordinal_indicators->n;
                ordinal_indicator_t ordinal_source = ordinal_indicator_rules[j];

                if (ordinal_source.key == NULL) {
                    log_error("ordinal source key was NULL at index %d\n", j);
                    exit(EXIT_FAILURE);
                }

                char *ordinal_indicator_key = strdup(ordinal_source.key);
                if (ordinal_indicator_key == NULL) {
                    log_error("Error in strdup\n");
                    exit(EXIT_FAILURE);
                }

                char *suffix = NULL;
                if (ordinal_source.suffix != NULL) {
                    suffix = strdup(ordinal_source.suffix);
                    if (suffix == NULL) {
                        log_error("Error in strdup\n");
                        exit(EXIT_FAILURE);
                    }
                }

                char_array_clear(key);
                char_array_cat(key, lang);

                if (!ordinal_phrases) {
                    ordinal_indicator_t *ordinal = ordinal_indicator_new(ordinal_indicator_key, ordinal_source.gender, ordinal_source.category, suffix);
                    ordinal_indicator_array_push(numex_table->ordinal_indicators, ordinal);            

                    char_array_cat(key, ORDINAL_NAMESPACE_PREFIX);
                } else {
                    char_array_cat(key, ORDINAL_PHRASE_NAMESPACE_PREFIX);
                }

                switch (ordinal_source.gender) {
                    case GENDER_MASCULINE:
                        char_array_cat(key, GENDER_MASCULINE_PREFIX);
                        break;
                    case GENDER_FEMININE:
                        char_array_cat(key, GENDER_FEMININE_PREFIX);
                        break;
                    case GENDER_NEUTER:
                        char_array_cat(key, GENDER_NEUTER_PREFIX);
                        break;
                    case GENDER_NONE:
                    default:
                        char_array_cat(key, GENDER_NONE_PREFIX);
                }

                switch (ordinal_source.category) {
                    case CATEGORY_PLURAL:
                        char_array_cat(key, CATEGORY_PLURAL_PREFIX);
                        break;
                    case CATEGORY_DEFAULT:
                    default:
                        char_array_cat(key, CATEGORY_DEFAULT_PREFIX);

                }

                char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);

                char *key_str = ordinal_source.key;

                if (ordinal_phrases) {
                    key_str = suffix;
                }

                char *reversed = utf8_reversed_string(key_str);
                char_array_cat(key, reversed);
                free(reversed);

                char *str_key = char_array_get_string(key);

                if (trie_get(numex_table->trie, str_key) == NULL_NODE_ID) {
                    trie_add(numex_table->trie, str_key, value);
                }
            }
        }

        char *name = strdup(lang_source.name);
        if (name == NULL) {
            log_error("Error in strdup\n");
            exit(EXIT_FAILURE);
        }

        numex_language_t *language = numex_language_new(name, lang_source.whole_tokens_only, lang_source.rule_index, lang_source.num_rules, lang_source.ordinal_indicator_index, lang_source.num_ordinal_indicators);
        numex_table_add_language(language);

    }

    char_array_destroy(key);

    if (!numex_table_write(f)) {
        log_error("Error writing numex table\n");
        exit(1);
    }

    fclose(f);

    numex_module_teardown();

    log_info("Done\n");
}
