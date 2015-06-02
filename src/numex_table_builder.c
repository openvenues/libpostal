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
        exit(1);
    }

    if (!numex_module_setup(NULL)) {
        log_error("Numex table initialization unsuccessful\n");
        exit(1);
    }

    numex_table_t *numex_table = get_numex_table();

    size_t num_languages = sizeof(numex_languages) / sizeof(numex_language_source_t);

    size_t num_source_rules = sizeof(numex_rules) / sizeof(numex_rule_source_t);
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
            numex_rule_source_t rule_source = numex_rules[j];

            value = rule_source.rule.rule_type != NUMEX_STOPWORD ? numex_table->rules->n : NUMEX_STOPWORD_INDEX;
            numex_rule_array_push(numex_table->rules, rule_source.rule);

            char_array_clear(key);
            char_array_cat(key, lang);
            char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);
            char_array_cat(key, rule_source.key);

            char *str_key = char_array_get_string(key);

            trie_add(numex_table->trie, str_key, value);
        }

        for (j = ordinal_indicator_index; j < ordinal_indicator_index + num_ordinal_indicators; j++) {
            ordinal_indicator_t ordinal_source = ordinal_indicator_rules[j];
            ordinal_indicator_t *ordinal = ordinal_indicator_new(ordinal_source.number, ordinal_source.gender, ordinal_source.suffix);
            ordinal_indicator_array_push(numex_table->ordinal_indicators, ordinal);            
        }

        numex_language_t *language = numex_language_new(lang_source.name, lang_source.rule_index, lang_source.num_rules, lang_source.ordinal_indicator_index, lang_source.num_ordinal_indicators);
        numex_table_add_language(language);

    }

    char_array_destroy(key);

    if (!numex_table_write(f)) {
        log_error("Error writing numex table\n");
        exit(1);
    }

    log_info("Done\n");
}
