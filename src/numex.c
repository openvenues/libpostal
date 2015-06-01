#include "numex.h"
#include "file_utils.h"

#define NUMEX_TABLE_SIGNATURE 0xBBBBBBBB

#define SEPARATOR_TOKENS "-"

numex_table_t *numex_table = NULL;

numex_table_t *get_numex_table(void) {
    return numex_table;
}

void numex_table_destroy(void) {
    numex_table_t *numex_table = get_numex_table();
    if (numex_table == NULL) return;

    if (numex_table->trie != NULL) {
        trie_destroy(numex_table->trie);
    }

    if (numex_table->languages != NULL) {
        numex_language_t *language;
        kh_foreach(numex_table->languages, language, {
            numex_language_destroy(language);
        })

        kh_destroy(str_numex_language, numex_table->languages);
    }

    if (numex_table->rules != NULL) {
        numex_rule_array_destroy(numex_table->rules);
    }

    if (numex_table->ordinal_indicators != NULL) {
        ordinal_indicator_array_destroy(numex_table->ordinal_indicators);
    }

    free(self);
}

numex_table_t *numex_table_init(void) {
    numex_table_t *numex_table = get_numex_table();

    if (numex_table == NULL) {
        numex_table = malloc(sizeof(numex_table_t));

        if (numex_table == NULL) return NULL;

        numex_table->trie = trie_new();
        if (numex_table->trie == NULL) {
            goto exit_numex_table_created;
        }


        numex_table->languages = kh_init(str_numex_language);
        if (numex_table->languages == NULL) {
            goto exit_numex_table_created;
        }

        numex_table->rules = numex_rule_array_new();
        if (numex_table->rules == NULL) {
            goto exit_numex_table_created;
        }

        numex_table->ordinal_indicators = ordinal_indicator_array_new();
        if (numex_table->ordinal_indicators == NULL) {
            goto exit_numex_table_created;
        }

    }

    return numex_table;
exit_numex_table_created:
    numex_table_destroy();
    exit(1);
}

numex_table_t *numex_table_new(void) {
    numex_table_t *numex_table = numex_table_init();
    if (numex_table != NULL) {
        numex_rule_array_push(numex_table->rules, NUMEX_STOPWORD_RULE);
    }
    return numex_table;
}


numex_language_t *numex_language_new(char *name, bool concatenated, size_t rules_index, size_t num_rules, size_t ordinals_index, size_t num_ordinals) {
    numex_language_t *language = malloc(sizeof(numex_language_t));
    if (language == NULL) return NULL;

    language->name = strdup(name);
    language->concatenated = concatenated;
    language->rules_index = rules_index;
    language->num_rules = num_rules;
    language->ordinals_index = ordinals_index;
    language->num_ordinals = num_ordinals;

    return language;
}

void numex_language_destroy(numex_language_t *self) {
    if (self == NULL) return;

    if (self->name != NULL) {
        free(self->name);
    }

    free(self);
}

bool numex_table_add_language(numex_language_t *language) {
    if (numex_table == NULL) {
        return false;
    }

    int ret;
    khiter_t k = kh_put(str_numex_language, numex_table->languages, language->name, &ret);
    kh_value(numex_table->languages, k) = language;

    return true;
}

numex_language_t *get_numex_language(char *name) {
    if (numex_table == NULL) {
        return NULL;
    }

    khiter_t k;
    k = kh_get(str_numex_language, numex_table->languages, name);
    return (k != kh_end(numex_table->languages) ? kh_value(numex_table->languages, k) : NULL);
}

numex_language_t *numex_language_read(FILE *f) {
    size_t lang_name_len;

    if (!file_read_uint64(f, (uint64_t *)&lang_name_len)) {
        return NULL;
    }

    char name[lang_name_len];

    if (!file_read_chars(f, name, lang_name_len)) {
        return NULL;
    }

    bool concatenated;
    if (!file_read_uint8(f, (uint8_t *)&concatenated)) {
        return NULL;
    }

    size_t rules_index;
    if (!file_read_uint64(f, (uint64_t *)&rules_index)) {
        return NULL;
    }

    size_t num_rules;
    if (!file_read_uint64(f, (uint64_t *)&num_rules)) {
        return NULL;
    }

    size_t ordinals_index;
    if (!file_read_uint64(f, (uint64_t *)&ordinals_index)) {
        return NULL;
    }

    size_t num_ordinals;
    if (!file_read_uint64(f, (uint64_t *)&num_ordinals)) {
        return NULL;
    }

    numex_language_t *language = numex_language_new(name, concatenated, rules_index, num_rules, ordinals_index, num_ordinals);

    return language;

}

bool numex_language_write(numex_language_t *language, FILE *f) {
    size_t lang_name_len = strlen(language->name) + 1;

    if (!file_write_uint64(f, (uint64_t)lang_name_len)) {
        return false;
    }

    if (!file_write_chars(f, language->name, lang_name_len)) {
        return false;
    }

    if (!file_write_uint8(f, language->concatenated)) {
        return false;
    }

    if (!file_write_uint64(f, language->rules_index)) {
        return false;
    }

    if (!file_write_uint64(f, language->num_rules)) {
        return false;
    }

    if (!file_write_uint64(f, language->ordinals_index)) {
        return false;
    }

    if (!file_write_uint64(f, language->num_ordinals)) {
        return false;
    }

    return true;

}

bool numex_rule_read(FILE *f, numex_rule_t *rule) {
    if (!file_read_uint64(f, rule->left_context_type)) {
        return false;
    }

    if (!file_read_uint64(f, rule->right_context_type)) {
        return false;
    }

    if (!file_read_uint64(f, rule->rule_type)) {
        return false;
    }

    if (!file_read_uint64(f, rule->gender)) {
        return false;
    }

    if (!file_read_uint32(f, rule->radix)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)rule->value)) {
        return false;
    }

    return true;
}

bool numex_rule_write(numex_rule_t rule, FILE *f) {
    if (!file_write_uint64(f, (uint64_t)rule.left_context_type)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)rule.right_context_type)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)rule.rule_type)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)rule.gender)) {
        return false;
    }

    if (!file_write_uint32(f, (uint32_t)rule.radix)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)rule.value)) {
        return false;
    }

    return true;
}

void ordinal_indicator_destroy(ordinal_indicator_t *self) {
    if (self == NULL) return;

    if (self->name != NULL) {
        free(self->name);
    }

    free(self);
}

ordinal_indicator_t *ordinal_indicator_new(uint8_t number, gender_t gender, char *name) {
    ordinal_indicator_t *ordinal = malloc(sizeof(ordinal_indicator_t));
    if (ordinal == NULL) {
        return NULL;
    }

    ordinal->name = strdup(name);
    if (ordinal->name == NULL) {
        ordinal_indicator_destroy(ordinal);
        return NULL;
    }

    ordinal->number = number;
    ordinal->gender = gender;

    return ordinal;
}

ordinal_indicator_t *ordinal_indicator_read(FILE *f) {
    uint8_t number;
    if (!file_read_uint8(f, &number)) {
        return NULL;
    }

    gender_t gender;
    if (!file_read_uint64(f, (uint64_t *)&gender)) {
        return NULL;
    }

    size_t ordinal_name_len;
    if (!file_read_uint64(f, &ordinal_name_len)) {
        return NULL;
    }

    char ordinal_name[ordinal_name_len];

    if (!file_read_chars(f, ordinal_name, ordinal_name_len)) {
        return NULL;
    }

    return ordinal_indicator_new(number, gender, ordinal_name);
}


bool ordinal_indicator_write(ordinal_indicator_t *ordinal, FILE *f) {
    if (!file_write_uint8(f, ordinal->number)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)ordinal->gender)) {
        return false;
    }

    size_t name_len = strlen(ordinal->name) + 1;
    if (!file_write_uint64(f, name_len) ||
        !file_write_chars(f, ordinal->name, name_len)) {
        return false;
    }

    return true;

}


bool numex_table_read(FILE *f) {
    if (f == NULL) {
        return false;
    }

    uint32_t signature;

    log_debug("Reading signature\n");

    if (!file_read_uint32(f, &signature) || signature != NUMEX_TABLE_SIGNATURE) {
        return false;
    }

    numex_table = numex_table_init();

    log_debug("Numex table initialized\n");

    size_t num_languages;

    if (!file_read_uint64(f, (uint64_t *)&num_languages)) {
        goto exit_numex_table_load_error;
    }

    int i = 0;

    numex_language_t *language;

    for (i = 0; i < num_languages; i++) {
        language = numex_language_read(f);
        if (language == NULL || !numex_table_add_language(language)) {
            goto exit_numex_table_load_error;
        }
    }

    size_t num_rules;

    if (!file_read_uint64(f, (uint64_t *)&num_rules)) {
        goto exit_numex_table_load_error;
    }

    numex_rule_t rule;

    for (i = 0; i < num_rules; i++) {
        if (!numex_rule_read(f, &rule)) {
            goto exit_numex_table_load_error;
        }
        numex_rule_array_push(numex_table->rules, rule);
    }

    size_t num_ordinals;

    if (!file_read_uint64(f, (uint64_t *)&num_ordinals)) {
        goto exit_numex_table_load_error;
    }

    ordinal_indicator_t *ordinal;

    for (i = 0; i < num_ordinals; i++) {
        ordinal = ordinal_indicator_read(f);
        if (ordinal == NULL) {
            goto exit_numex_table_load_error;
        }
        ordinal_indicator_array_push(numex_table->ordinal_indicators, ordinal);
    }

    numex_table->trie = trie_read(f);
    if (numex_table->trie == MNULL) {
        goto exit_numex_table_load_error;
    }

    return true;

exit_numex_table_load_error:
    numex_table_destroy();
    return false;
}

numex_table_t *numex_table_load(char *filename) {
    FILE *f;
    if ((f = fopen(filename, "rb")) == NULL) {
        return NULL;
    }
    return numex_table_read(f);
}

bool numex_table_write(FILE *f) {
    if (!file_write_uint32(f, (uint32_t)NUMEX_TABLE_SIGNATURE)) {
        return false;
    }

    size_t num_languages = kh_size(numex_table->languages);

    if (!file_write_uint64(f, (uint64_t)num_languages)) {
        return false;
    }

    numex_language_t *language;

    kh_foreach_value(numex_table->languages, language, {
        if (!numex_language_write(language, f)) {
            return false;
        }
    })

    size_t num_rules = numex_table->rules->n;

    if (!file_write_uint64(f, (uint64_t)num_rules)) {
        return false;
    }

    numex_rule_t rule;

    for (i = 0; i < num_rules; i++) {
        rule = numex_table->rules->a[i];

        if (!numex_rule_write(rule, f)) {
            return false;
        }
    }

    size_t num_ordinals = numex_table->ordinal_indicators->n;

    if (!file_write_uint64(f, (uint64_t)num_ordinals)) {
        return false;
    }

    ordinal_indicator_t *ordinal;

    for (i = 0; i < num_ordinals; i++) {
        ordinal = numex_table->ordinal_indicators->a[i];

        if (!ordinal_indicator_write(ordinal, f)) {
            return false;
        }
    }

    if (!trie_write(numex_table->trie, f)) {
        return false;
    }

    return true;
}

bool numex_table_save(char *filename) {
    if (numex_table == NULL || filename == NULL) {
        return false;
    }

    FILE *f;

    if ((f = fopen(filename, "wb")) != NULL) {
        bool ret = numex_table_write(f);
        fclose(f);
        return ret;
    } else {
        return false;
    }
}

/* Initializes numex trie/module
Must be called only once before the module can be used
*/
void numex_module_setup(char *filename) {
    if (filename == NULL) {
        numex_table_new();
    } else if (numex_table == NULL) {
        numex_table = numex_table_load(filename);
    }

}

/* Teardown method for the module
Called once when done with the module (usually at
the end of a main method)
*/
void numex_module_teardown(void) {
    numex_table_destroy();
}