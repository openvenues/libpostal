#include <math.h>
#include "numex.h"
#include "file_utils.h"

#define NUMEX_TABLE_SIGNATURE 0xBBBBBBBB

#define LATIN_LANGUAGE_CODE "la"

#define SEPARATOR_TOKENS "-"

#define FLOOR_LOG_BASE(num, base) floor((log((float)num) / log((float)base)) + 0.00001)

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
        kh_foreach_value(numex_table->languages, language, {
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

    free(numex_table);
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
        numex_rule_t null_rule = NUMEX_NULL_RULE;
        numex_rule_array_push(numex_table->rules, null_rule);
        numex_rule_t stopword_rule = NUMEX_STOPWORD_RULE;
        numex_rule_array_push(numex_table->rules, stopword_rule);
    }
    return numex_table;
}


numex_language_t *numex_language_new(char *name, bool whole_tokens_only, size_t rules_index, size_t num_rules, size_t ordinals_index, size_t num_ordinals) {
    numex_language_t *language = malloc(sizeof(numex_language_t));
    if (language == NULL) return NULL;

    language->name = strdup(name);
    language->whole_tokens_only = whole_tokens_only;
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
    return k != kh_end(numex_table->languages) ? kh_value(numex_table->languages, k) : NULL;
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

    bool whole_tokens_only;
    if (!file_read_uint8(f, (uint8_t *)&whole_tokens_only)) {
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

    numex_language_t *language = numex_language_new(name, whole_tokens_only, rules_index, num_rules, ordinals_index, num_ordinals);

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

    if (!file_write_uint8(f, language->whole_tokens_only)) {
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
    if (!file_read_uint64(f, (uint64_t *)&rule->left_context_type)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)&rule->right_context_type)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)&rule->rule_type)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)&rule->gender)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)&rule->category)) {
        return false;
    }

    if (!file_read_uint32(f, &rule->radix)) {
        return false;
    }

    if (!file_read_uint64(f, (uint64_t *)&rule->value)) {
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

    if (!file_write_uint64(f, (uint64_t)rule.category)) {
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

    if (self->key != NULL) {
        free(self->key);
    }

    if (self->suffix != NULL) {
        free(self->suffix);
    }

    free(self);
}

ordinal_indicator_t *ordinal_indicator_new(char *key, gender_t gender, grammatical_category_t category, char *suffix) {
    ordinal_indicator_t *ordinal = malloc(sizeof(ordinal_indicator_t));
    if (ordinal == NULL) {
        return NULL;
    }

    ordinal->key = strdup(key);
    if (ordinal->key == NULL) {
        ordinal_indicator_destroy(ordinal);
        return NULL;
    }

    ordinal->suffix = strdup(suffix);
    if (ordinal->suffix == NULL) {
        ordinal_indicator_destroy(ordinal);
        return NULL;
    }

    ordinal->category = category;
    ordinal->gender = gender;

    return ordinal;
}

ordinal_indicator_t *ordinal_indicator_read(FILE *f) {
    size_t key_len;
    if (!file_read_uint64(f, (uint64_t *)&key_len)) {
        return NULL;
    }

    char key[key_len];

    if (!file_read_chars(f, key, key_len)) {
        return NULL;
    }

    gender_t gender;
    if (!file_read_uint64(f, (uint64_t *)&gender)) {
        return NULL;
    }

    grammatical_category_t category;
    if (!file_read_uint64(f, (uint64_t *)&category)) {
        return NULL;
    }

    size_t ordinal_suffix_len;
    if (!file_read_uint64(f, (uint64_t *)&ordinal_suffix_len)) {
        return NULL;
    }

    char ordinal_suffix[ordinal_suffix_len];

    if (!file_read_chars(f, ordinal_suffix, ordinal_suffix_len)) {
        return NULL;
    }

    return ordinal_indicator_new(key, gender, category, ordinal_suffix);
}


bool ordinal_indicator_write(ordinal_indicator_t *ordinal, FILE *f) {
    size_t key_len = strlen(ordinal->key) + 1;
    if (!file_write_uint64(f, key_len) ||
        !file_write_chars(f, ordinal->key, key_len)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)ordinal->gender)) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)ordinal->category)) {
        return false;
    }

    size_t name_len = strlen(ordinal->suffix) + 1;
    if (!file_write_uint64(f, name_len) ||
        !file_write_chars(f, ordinal->suffix, name_len)) {
        return false;
    }

    return true;

}


bool numex_table_read(FILE *f) {
    if (f == NULL) {
        log_warn("FILE pointer was NULL in numex_table_read\n");
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

    log_debug("read num_languages = %d\n", num_languages);

    int i = 0;

    numex_language_t *language;

    for (i = 0; i < num_languages; i++) {
        language = numex_language_read(f);
        if (language == NULL || !numex_table_add_language(language)) {
            goto exit_numex_table_load_error;
        }
    }

    log_debug("read languages\n");


    size_t num_rules;

    if (!file_read_uint64(f, (uint64_t *)&num_rules)) {
        goto exit_numex_table_load_error;
    }

    log_debug("read num_rules = %zu\n", num_rules);

    numex_rule_t rule;

    for (i = 0; i < num_rules; i++) {
        if (!numex_rule_read(f, &rule)) {
            goto exit_numex_table_load_error;
        }
        numex_rule_array_push(numex_table->rules, rule);
    }

    log_debug("read rules\n");

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

    trie_destroy(numex_table->trie);

    numex_table->trie = trie_read(f);
    if (numex_table->trie == NULL) {
        goto exit_numex_table_load_error;
    }

    log_debug("read trie\n");

    return true;

exit_numex_table_load_error:
    numex_table_destroy();
    return false;
}

bool numex_table_load(char *filename) {
    FILE *f;
    if ((f = fopen(filename, "rb")) == NULL) {
        return NULL;
    }
    bool ret = numex_table_read(f);
    fclose(f);
    return ret;
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

    int i = 0;

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
bool numex_module_setup(char *filename) {
    if (filename == NULL) {
        numex_table = numex_table_new();
        return numex_table != NULL;
    } else if (numex_table == NULL) {
        return numex_table_load(filename);
    }
    return false;
}

/* Teardown method for the module
Called once when done with the module (usually at
the end of a main method)
*/
void numex_module_teardown(void) {
    numex_table_destroy();
    numex_table = NULL;
}

#define NULL_NUMEX_RESULT (numex_result_t) {0, GENDER_NONE, CATEGORY_DEFAULT, false, 0, 0}

typedef enum {
    NUMEX_SEARCH_STATE_BEGIN,
    NUMEX_SEARCH_STATE_SKIP_TOKEN,
    NUMEX_SEARCH_STATE_PARTIAL_MATCH,
    NUMEX_SEARCH_STATE_MATCH
} numex_search_state_type;

typedef struct numex_search_state {
    uint32_t node_id;
    numex_search_state_type state;
} numex_search_state_t;

#define NULL_NUMEX_SEARCH_STATE (numex_search_state_t) {NULL_NODE_ID, NUMEX_SEARCH_STATE_BEGIN}


static inline numex_rule_t get_numex_rule(size_t i) {
    if (i >= numex_table->rules->n) return NUMEX_NULL_RULE;
    return numex_table->rules->a[i];
}

numex_result_array *convert_numeric_expressions(char *str, char *lang) {
    if (numex_table == NULL) return NULL;

    trie_t *trie = numex_table->trie;
    if (trie == NULL) return NULL;

    numex_language_t *language = get_numex_language(lang);

    if (language == NULL) return NULL;

    bool whole_tokens_only = language->whole_tokens_only;

    trie_prefix_result_t prefix = trie_get_prefix(trie, lang);

    if (prefix.node_id == NULL_NODE_ID) {
        return NULL;
    }

    prefix = trie_get_prefix_from_index(trie, NAMESPACE_SEPARATOR_CHAR, NAMESPACE_SEPARATOR_CHAR_LEN, prefix.node_id, prefix.tail_pos);

    if (prefix.node_id == NULL_NODE_ID) {
        return NULL;
    }

    numex_result_t prev_result = NULL_NUMEX_RESULT;
    numex_result_t result = prev_result;

    numex_result_array *results = NULL;

    numex_rule_t prev_rule = NUMEX_NULL_RULE;
    numex_rule_t rule = prev_rule;

    numex_search_state_t state = NULL_NUMEX_SEARCH_STATE;

    numex_search_state_t start_state = NULL_NUMEX_SEARCH_STATE;
    uint32_t start_node_id = prefix.node_id;
    start_state.node_id = start_node_id;

    numex_search_state_t prev_state = start_state;

    size_t len = strlen(str);
    size_t idx = 0;

    int32_t codepoint = 0;
    ssize_t char_len = 0;
    uint8_t *ptr = (uint8_t *)str;
    unsigned char ch = '\0';

    bool advance_index = true;
    bool advance_state = true;

    bool number_finished = false;

    bool is_space = false;
    bool is_hyphen = false;

    char_array *number_str = NULL;

    bool last_was_separator = false;
    bool possible_complete_token = false;
    bool complete_token = false;

    while (idx < len) {
        if (state.state == NUMEX_SEARCH_STATE_SKIP_TOKEN) {
            char_len = utf8proc_iterate(ptr, len, &codepoint);
            int cat = utf8proc_category(codepoint);

            if (codepoint == 0) break;

            is_space = utf8_is_separator(cat);
            if (is_space) {
                log_debug("is_space\n");
                is_hyphen = false;
            } else {
                is_hyphen = utf8_is_hyphen(codepoint);
                if (is_hyphen) {
                    log_debug("is_hyphen\n");
                }
            }

            idx += char_len;
            ptr += char_len;

            if (is_space || is_hyphen) {
                state = start_state;
                last_was_separator = true;
                if (possible_complete_token) {
                    log_debug("Complete token\n");
                    complete_token = true;
                    possible_complete_token = false;
                } else if (prev_state.state == NUMEX_SEARCH_STATE_MATCH) {
                    log_debug("Complete token\n");
                    complete_token = true;
                    prev_state = NULL_NUMEX_SEARCH_STATE;
                } else {
                    complete_token = false;
                }
            } else if (whole_tokens_only && last_was_separator) {
                log_debug("last was separator\n");
                last_was_separator = false;
                possible_complete_token = true;
            } else {
                log_debug("other char\n");
                if (result.len > 0 && (!whole_tokens_only || complete_token)) {
                    results = (results != NULL) ? results : numex_result_array_new_size(1);
                    numex_result_array_push(results, result);
                    log_debug("Adding phrase from partial token, value=%lld\n", result.value);
                    prev_rule = rule = NUMEX_NULL_RULE;
                }
                result = NULL_NUMEX_RESULT;
                last_was_separator = false;
                possible_complete_token = false;
                complete_token = false;
            }
            continue;
        }

        phrase_t phrase = trie_search_prefixes_from_index(trie, str + idx, start_node_id);

        state = start_state;

        if (phrase.len == 0) {
            log_debug("phrase.len == 0, skipping token\n");
            state.state = NUMEX_SEARCH_STATE_SKIP_TOKEN;
            continue;
        }

        uint32_t rule_index = phrase.data;

        bool set_rule = false;
        state.state = NUMEX_SEARCH_STATE_MATCH;

        log_debug("phrase.len=%lld, phrase.data=%d\n", phrase.len, phrase.data);

        rule = get_numex_rule((size_t)phrase.data);
        log_debug("rule.value=%lld\n", rule.value);

        if (rule.rule_type != NUMEX_NULL) {
            set_rule = true;

            if (rule.gender != GENDER_NONE) {
                result.gender = rule.gender;
            }

            if (rule.category != CATEGORY_DEFAULT) {
                result.category = rule.category;
            }

            /* e.g. in English, "two hundred", when you get to hundred, multiply by the 
               left value mod the current value, which also covers things like
               "one thousand two hundred" although in those cases should be less commmon in addresses
            */

            if (result.len == 0) {
                result.start = idx + phrase.start;
            }
            result.len = idx + phrase.start + phrase.len - result.start;

            log_debug("ide=%d, phrase.len=%d\n", idx, phrase.len);

            log_debug("prev_rule.radix=%d\n", prev_rule.radix);

            if (rule.left_context_type == NUMEX_LEFT_CONTEXT_MULTIPLY) {
                int64_t multiplier = result.value % rule.value;
                if (multiplier != 0) {
                    result.value -= multiplier;
                } else {
                    multiplier = 1;
                }
                result.value += rule.value * multiplier;
                log_debug("LEFT_CONTEXT_MULTIPLY, value = %lld\n", result.value);
            } else if (rule.left_context_type == NUMEX_LEFT_CONTEXT_ADD) {
                result.value += rule.value;
                log_debug("LEFT_CONTEXT_ADD, value = %lld\n", result.value);
            } else if (prev_rule.right_context_type == NUMEX_RIGHT_CONTEXT_ADD && rule.value > 0 && prev_rule.radix > 0 && FLOOR_LOG_BASE(rule.value, prev_rule.radix) < FLOOR_LOG_BASE(prev_rule.value, prev_rule.radix)) {
                result.value += rule.value;
                log_debug("Last token was RIGHT_CONTEXT_ADD, value=%lld\n", result.value);
            } else if (prev_rule.rule_type != NUMEX_NULL && rule.rule_type != NUMEX_STOPWORD) {
                log_debug("Had previous token with no context, finishing previous rule before returning\n");

                number_finished = true;
                advance_index = false;
                state = start_state;
                rule = prev_rule = NUMEX_NULL_RULE;
            } else if (rule.rule_type != NUMEX_STOPWORD) {
                result.value = rule.value;
                log_debug("Got number, result.value=%lld\n", result.value);
            }

            if (rule.rule_type != NUMEX_STOPWORD) {
                prev_rule = rule;
            }

            if (rule.rule_type == NUMEX_ORDINAL_RULE) {
                result.is_ordinal = true;
                if (rule.right_context_type == NUMEX_RIGHT_CONTEXT_NONE) {
                    number_finished = true;
                }
                log_debug("rule is ordinal\n");
            } 

            if (idx + phrase.len == len) {
                number_finished = true;
            }
        }
        if (!set_rule) {
            rule = prev_rule = NUMEX_NULL_RULE;
            log_debug("Resetting rules to NUMEX_NULL_RULE\n");
        }

        set_rule = false;

        if (advance_index) {
            idx += phrase.len;
            ptr += phrase.len;
        }

        advance_index = true;

        if (number_finished) {
            results = (results != NULL) ? results : numex_result_array_new_size(1);
            numex_result_array_push(results, result);
            log_debug("Adding phrase, value=%lld\n", result.value);
            result = NULL_NUMEX_RESULT;
            number_finished = false;
        }

        prev_state = state;

    }



        /*





        } else if (!is_space && !is_hyphen) {
            log_info("Tail did not match\n");
            state = start_state;
            if (number_finished) {

            }
        }

        last_node = start_node;
        last_node_id = start_state.node_id;

        check_match = false;

        }





        state = start_state;
 
        uint8_t *back_ptr = ptr;

        bool check_match = false;

        for (int i = 0; remaining > 0; remaining--, ptr++) {
            log_debug("start loop\n");
            ch = (unsigned char) *ptr;
            log_debug("char=%c, last_node_id=%d\n", ch, last_node_id);

            node_id = trie_get_transition_index(trie, last_node, ch);
            node = trie_get_node(trie, node_id);

            if (node.check != last_node_id) {
                log_debug("node.check != last_node_id\n");
                uint32_t match_id = trie_get_transition_index(trie, last_node, '\0');
                trie_node_t match_node = trie_get_node(trie, match_id);
                if (match_node.check != last_node_id) {
                    state = start_state;
                    last_node = start_node;
                    last_node_id = start_state.node_id;

                    log_debug("No NUL-byte transition, resetting state to start node_id=%d\n", last_node_id);

                    if (!is_space && !is_hyphen) {
                        log_debug("Fell off trie inside token. Setting to skip\n");
                        state.state = NUMEX_SEARCH_STATE_SKIP_TOKEN;
                        ptr += remaining;
                        rule = prev_rule = NUMEX_NULL_RULE;
                        if (prev_state.state == NUMEX_SEARCH_STATE_MATCH) {
                            log_debug("Previous number was match\n");
                            number_finished = true;
                        }
                        break;
                    } else if (prev_state.state == NUMEX_SEARCH_STATE_MATCH) {
                        state = prev_state;
                    }

                } else {
                    log_debug("Have NUL-byte transition\n");

                    check_match = true;
                    node_id = match_id;
                    node = match_node;
                    last_node = start_node;
                    last_node_id = start_state.node_id;
                    remaining = 0;
                    advance_index = false;
                }

            } else {

                log_debug("not null\n");
                state.state = NUMEX_SEARCH_STATE_PARTIAL_MATCH;
                if (phrase.len == 0) {
                    log_debug("phrase.start=%d\n", idx);
                    phrase.start = idx;
                    phrase.len = char_len;
                }

                if (node.base >= 0) {
                    last_node = node;
                    last_node_id = node_id;
                } else if (node.base < 0) {
                    log_debug("node.base < 0\n");
                    remaining--;
                    check_match = true;
                }
            }


            if (check_match) {

                trie_data_node_t data_node = trie_get_data_node(trie, node);

                unsigned char *current_tail = trie->tail->a + data_node.tail;

                size_t tail_len = strlen((char *)current_tail);
                char *query_tail = (char *)(*ptr ? ptr + 1 : ptr);
                size_t query_tail_len = strlen((char *)query_tail);

                log_info("query_tail=%s, current_tail=%s, bytes=%zu\n", query_tail, current_tail, tail_len);

                if (tail_len <= query_tail_len && utf8_compare_len_ignore_separators((char *)current_tail, query_tail, tail_len) == 0) {
                    bool set_rule = false;
                    state.state = NUMEX_SEARCH_STATE_MATCH;

                    phrase.len = idx - phrase.start + tail_len;
                    log_info("phrase.start=%d\n, idx=%d, phrase.len=%d\n", phrase.start, idx, phrase.len);

                    ptr += remaining + tail_len;
                    log_info("remaining=%d, tail_len=%d\n", remaining, tail_len);

                    char_len += remaining + tail_len;
                    remaining = 0;

                    rule = get_numex_rule((size_t)data_node.data);

                    log_info("rule.value=%lld\n", rule.value);

                    if (rule.rule_type != NUMEX_NULL) {
                        set_rule = true;

                        if (rule.gender != GENDER_NONE) {
                            phrase.gender = rule.gender;
                        }

                        if (rule.category != CATEGORY_DEFAULT) {
                            phrase.category = rule.category;
                        }

                        if (rule.rule_type == NUMEX_ORDINAL_RULE) {
                            phrase.is_ordinal = true;
                            number_finished = true;
                            log_info("rule is ordinal\n");
                        } 

                        log_debug("prev_rule.radix=%d\n", prev_rule.radix);

                        if (rule.left_context_type == NUMEX_LEFT_CONTEXT_MULTIPLY) {
                            int64_t multiplier = phrase.value % rule.value;
                            if (multiplier != 0) {
                                phrase.value -= multiplier;
                            } else {
                                multiplier = 1;
                            }
                            phrase.value += rule.value * multiplier;
                            log_debug("LEFT_CONTEXT_MULTIPLY, value = %lld\n", phrase.value);
                        } else if (rule.left_context_type == NUMEX_LEFT_CONTEXT_ADD) {
                            phrase.value += rule.value;
                            log_debug("LEFT_CONTEXT_ADD, value = %lld\n", phrase.value);
                        } else if (prev_rule.right_context_type == NUMEX_RIGHT_CONTEXT_ADD && rule.value > 0 && prev_rule.radix > 0 && FLOOR_LOG_BASE(rule.value, prev_rule.radix) < FLOOR_LOG_BASE(prev_rule.value, prev_rule.radix)) {
                            phrase.value += rule.value;
                            log_debug("Last token was RIGHT_CONTEXT_ADD, value=%lld\n", phrase.value);
                        } else if (prev_rule.rule_type != NUMEX_NULL && rule.rule_type != NUMEX_STOPWORD) {
                            log_debug("Had previous token with no context, finishing previous rule before returning\n");

                            number_finished = true;
                            advance_index = false;
                            state = start_state;
                            last_node = start_node;
                            last_node_id = start_state.node_id;
                            rule = prev_rule = NUMEX_NULL_RULE;
                            break;
                        } else if (rule.rule_type != NUMEX_STOPWORD) {
                            phrase.value = rule.value;
                            log_debug("Got number, phrase.value=%lld\n", phrase.value);
                        }

                        if (rule.rule_type != NUMEX_STOPWORD) {
                            prev_rule = rule;
                        }
                    }
                    if (!set_rule) {
                        rule = prev_rule = NUMEX_NULL_RULE;
                        log_info("Resetting\n");
                    }

                    set_rule = false;
                } else if (!is_space && !is_hyphen) {
                    log_info("Tail did not match\n");
                    state = start_state;
                    if (number_finished) {

                    }
                }

                last_node = start_node;
                last_node_id = start_state.node_id;

                check_match = false;

            }


        }

        if (advance_index) {
            idx += char_len;
        } else {
            ptr = (uint8_t *)back_ptr;
        }

        if (number_finished) {
            phrases = (phrases != NULL) ? phrases : numex_phrase_array_new_size(1);
            numex_phrase_array_push(phrases, phrase);
            log_info("Adding phrase, value=%lld\n", phrase.value);
            phrase = NULL_NUMEX_PHRASE;
            number_finished = false;
        }

        
        prev_state = state;

        advance_index = true;

        log_debug("ptr=%s\n", ptr);

    }
    */

    return results;
}
