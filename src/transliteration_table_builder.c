/* transliteration_table_create.c
Creates the transliteration data structures from generated rule file.
Only used once at setup/make time, not overly concerned with optimization
*/

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"
#include "log/log.h"
#include "string_utils.h"
#include "trie.h"
#include "transliterate.h"
#include "transliteration_rule.h"
#include "transliteration_data.c"
#include "transliteration_scripts_data.c"

#include "utf8proc/utf8proc.h"

string_tree_t *regex_string_tree(char *regex, size_t len) {
    uint8_t *char_ptr = (uint8_t *)regex;
    bool in_set = false;
    bool in_brackets = false;

    int32_t codepoint;
    int32_t last_codepoint = 0;
    ssize_t char_len;

    size_t bracket_start;
    size_t bracket_len;

    char temp_char[MAX_UTF8_CHAR_SIZE];
    ssize_t temp_char_len;

    string_tree_t *tree = string_tree_new();

    if (len == 0) {
        // Single token with zero-length
        string_tree_add_string_len(tree, regex, len);
        string_tree_finalize_token(tree);
        return tree;
    }

    uint32_array *char_set = uint32_array_new();

    size_t idx = 0;

    int i, j;

    bool add_to_index = false;

    while (idx < len) {
        char_len = utf8proc_iterate(char_ptr, len, &codepoint);
        if (char_len <= 0) {
            uint32_array_destroy(char_set);
            string_tree_destroy(tree);
            return NULL;
        }

        if (!(utf8proc_codepoint_valid(codepoint))) {
            idx += char_len;
            char_ptr += char_len;
            continue;
        }

        add_to_index = true;

        if (codepoint == LSQUARE_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("begin set\n");
            in_set = true;
            codepoint = BEGIN_SET_CODEPOINT;
            uint32_array_clear(char_set);
        } else if (codepoint == RSQUARE_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT && in_set) {
            log_debug("end set");

            for (j = 0; j < char_set->n; j++) {
                temp_char_len = utf8proc_encode_char(char_set->a[j], (uint8_t *)temp_char);
                log_debug("Adding string %.*s\n", (int)temp_char_len, temp_char);
                string_tree_add_string_len(tree, temp_char, temp_char_len);
            }
            string_tree_finalize_token(tree);

            uint32_array_clear(char_set);
            // Add a special codepoint to the sequence to distinguish from an escaped square bracket
            codepoint = END_SET_CODEPOINT;
            in_set = false;
        } else if (codepoint == LCURLY_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            in_brackets = true;
            bracket_start = idx + char_len;
            bracket_len = 0;
            add_to_index = false;
        } else if (codepoint == RCURLY_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT && in_brackets) {
            log_debug("Adding bracketed string: %.*s\n", (int) bracket_len, regex + bracket_start);
            string_tree_add_string_len(tree, regex + bracket_start, bracket_len);
            in_brackets = false;
        } else if ((codepoint == LPAREN_CODEPOINT || codepoint == RPAREN_CODEPOINT) && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("group\n");
            add_to_index = false;
        } else if (in_set) {
            log_debug("in set\n");
            // Queue node, we'll add them to the trie
            uint32_array_push(char_set, codepoint);
            add_to_index = false;
        } else if (in_brackets) {
            add_to_index = false;
            bracket_len += char_len;
        } else if (codepoint == BACKSLASH_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            add_to_index = false;
        }

        log_debug("codepoint = %d\n", codepoint);

        if (add_to_index) {
            temp_char_len = utf8proc_encode_char(codepoint, (uint8_t *)temp_char);
            log_debug("char = %.*s\n", (int)temp_char_len, temp_char);
            string_tree_add_string_len(tree, temp_char, temp_char_len);
            string_tree_finalize_token(tree);
        }

        idx += char_len;
        char_ptr += char_len;
    }

    uint32_array_destroy(char_set);

    return tree;
   
}

group_capture_array *parse_groups(char *regex, size_t len) {
    uint8_t *char_ptr = (uint8_t *)regex;
    char last_ch = '\0';
    bool in_group = false;
    bool in_set = false;

    int32_t codepoint, last_codepoint = 0;
    ssize_t char_len;

    char temp_char[MAX_UTF8_CHAR_SIZE];
    ssize_t temp_char_len;


    if (len == 0) {
        return NULL;
    }

    group_capture_array *groups = group_capture_array_new_size(1);

    size_t idx = 0;

    size_t pos = 0;
    size_t group_start = 0;
    size_t chars_in_group = 0;

    while (idx < len) {
        char_len = utf8proc_iterate(char_ptr, len, &codepoint);
        if (char_len <= 0) {
            log_error("char %s had len=%zd\n", char_ptr, char_len);
            return NULL;
        }

        if (!(utf8proc_codepoint_valid(codepoint))) {
            idx += char_len;
            char_ptr += char_len;
            pos++;
            continue;
        }

        if (codepoint == LSQUARE_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("begin set\n");
            in_set = true;
        } else if (codepoint == RSQUARE_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("end set");
            pos++;
            in_set = false;
        } else if (codepoint == LPAREN_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("begin group\n");
            in_group = true;
            group_start = pos;
        } else if (codepoint == RPAREN_CODEPOINT && last_codepoint != BACKSLASH_CODEPOINT) {
            log_debug("close group\n");
            in_group = false;
            group_capture_array_push(groups, (group_capture_t){group_start, pos - group_start});
        } else if (!in_set) {
            log_debug("other char\n");
            pos++;
        }

        idx += char_len;
        char_ptr += char_len;

    }

    return groups;

}


// Methods used by trie builder and setup/teardown
bool transliteration_table_add_step(transliteration_table_t *self, step_type_t type, char *name) {
    transliteration_step_t *step = transliteration_step_new(name, type);

    step_array_push(self->steps, step);
    return true;
}

int main(int argc, char **argv) {
    char *filename;

    if (argc == 2) {
        filename = argv[1];
    } else {
        filename = DEFAULT_TRANSLITERATION_PATH;
    }

    FILE *f = fopen(filename, "wb");

    if (f == NULL) {
        log_error("File could not be opened, ensure directory exists: %s", filename);
        exit(1);
    }

    size_t num_source_transliterators = sizeof(transliterators_source) / sizeof(transliterator_source_t);

    char *key;
    size_t key_len;

    context_type_t pre_context_type;
    size_t pre_context_max_len;
    char *pre_context;
    size_t pre_context_len;

    context_type_t post_context_type;
    size_t post_context_max_len;
    char *post_context;
    size_t post_context_len;

    char *replacement;
    size_t replacement_len;

    char *revisit;
    size_t revisit_len;

    char *group_regex_str;
    size_t group_regex_len;

    transliteration_module_init();

    transliteration_table_t *trans_table = get_transliteration_table();

    trie_t *trie = trans_table->trie;

    for (int i = 0; i < num_source_transliterators; i++) {
        transliterator_source_t trans_source = transliterators_source[i];

        size_t trans_name_len = strlen(trans_source.name);

        log_info("Doing transliterator: %s\n", trans_source.name);

        char_array *trans_key = char_array_from_string(trans_source.name);
        char_array_cat(trans_key, NAMESPACE_SEPARATOR_CHAR);

        char *trans_name = strdup(trans_source.name);
        if (trans_name == NULL) {
            log_error("strdup returned NULL on trans_source.name\n");
            goto exit_teardown;
        }

        transliterator_t *trans = transliterator_new(trans_name, trans_source.internal, trans_table->steps->n, trans_source.steps_length);

        for (int j = 0; j < trans_source.steps_length; j++) {
            transliteration_step_source_t step_source = steps_source[trans_source.steps_start + j];

            size_t step_name_len = strlen(step_source.name);

            log_debug("Doing step: %s, type=%d\n", step_source.name, step_source.type);

            if (!transliteration_table_add_step(trans_table, step_source.type, step_source.name)) {
                log_error("Step couldn't be added\n");
                goto exit_teardown;
            }

            if (step_source.type != STEP_RULESET) {
                continue;
            }

            char_array *step_key = char_array_from_string(char_array_get_string(trans_key));
            char_array_cat(step_key, step_source.name);
            char_array_cat(step_key, NAMESPACE_SEPARATOR_CHAR);

            char *step_key_str = char_array_get_string(step_key);
            size_t step_key_len = strlen(step_key_str);

            for (int k = 0; k < step_source.rules_length; k++) {
                transliteration_rule_source_t rule_source = rules_source[step_source.rules_start + k];
                key = rule_source.key;
                key_len = rule_source.key_len;

                pre_context_type = rule_source.pre_context_type;
                pre_context_max_len = rule_source.pre_context_max_len;
                pre_context = rule_source.pre_context;
                pre_context_len = rule_source.pre_context_len;

                post_context_type = rule_source.post_context_type;
                post_context_max_len = rule_source.post_context_max_len;
                post_context = rule_source.post_context;
                post_context_len = rule_source.post_context_len;

                replacement = rule_source.replacement;
                replacement_len = rule_source.replacement_len;

                revisit = rule_source.revisit;
                revisit_len = rule_source.revisit_len;

                group_regex_str = rule_source.group_regex_str;
                group_regex_len = rule_source.group_regex_len;

                uint32_t data = trans_table->replacements->n;
                
                char_array *rule_key = char_array_from_string(step_key_str);

                uint32_t replacement_string_index = cstring_array_num_strings(trans_table->replacement_strings);
                cstring_array_add_string_len(trans_table->replacement_strings, replacement, replacement_len);

                uint32_t revisit_index = 0;
                if (revisit != NULL && revisit_len > 0) {
                    revisit_index = cstring_array_num_strings(trans_table->revisit_strings);
                    cstring_array_add_string_len(trans_table->revisit_strings, revisit, revisit_len);
                }

                group_capture_array *groups = parse_groups(group_regex_str, group_regex_len);

                transliteration_replacement_t *trans_repl = transliteration_replacement_new(replacement_string_index, revisit_index, groups);

                uint32_t replacement_index = trans_table->replacements->n;
                transliteration_replacement_array_push(trans_table->replacements, trans_repl);

                int c;

                char *token;

                log_debug("Doing rule: %s\n", key);

                string_tree_t *tree = regex_string_tree(key, key_len);

                string_tree_t *pre_context_tree = NULL;
                string_tree_iterator_t *pre_context_iter = NULL;

                cstring_array *pre_context_strings = NULL;

                if (pre_context_type != CONTEXT_TYPE_NONE) {
                    pre_context_strings = cstring_array_new();
                }

                if (pre_context_type == CONTEXT_TYPE_REGEX) {
                    log_debug("pre_context_type == CONTEXT_TYPE_REGEX\n");
                    pre_context_tree = regex_string_tree(pre_context, pre_context_len);

                    pre_context_iter = string_tree_iterator_new(pre_context_tree);

                    char_array *pre_context_perm = char_array_new_size(pre_context_len);

                    for (; !string_tree_iterator_done(pre_context_iter); string_tree_iterator_next(pre_context_iter)) {
                        char_array_clear(pre_context_perm);
                        for (c = 0; c < pre_context_iter->num_tokens; c++) {
                            token = string_tree_iterator_get_string(pre_context_iter, c);
                            if (token == NULL || strlen(token) == 0) {
                                log_warn("pre_token_context is NULL or 0 length: %s\n", token);
                            }
                            char_array_cat(pre_context_perm, token);
                        }
                        token = char_array_get_string(pre_context_perm);
                        if (token == NULL || strlen(token) == 0) {
                            log_warn("pre_perm is NULL or 0 length\n");
                        }
                        cstring_array_add_string(pre_context_strings, token);
                    }

                    char_array_destroy(pre_context_perm);
                    string_tree_iterator_destroy(pre_context_iter);
                    string_tree_destroy(pre_context_tree);
                } else if (pre_context_type == CONTEXT_TYPE_STRING) {
                    if (pre_context == NULL || strlen(pre_context) == 0) {
                        log_warn("pre_context STRING NULL or 0 length\n");
                    }
                    cstring_array_add_string(pre_context_strings, pre_context);
                } else if (pre_context_type == CONTEXT_TYPE_WORD_BOUNDARY) {
                    cstring_array_add_string(pre_context_strings, WORD_BOUNDARY_CHAR);
                }

                size_t num_pre_context_strings = 0;
                if (pre_context_type != CONTEXT_TYPE_NONE) {
                    num_pre_context_strings = cstring_array_num_strings(pre_context_strings);
                    log_debug("num_pre_context_strings = %zu\n", num_pre_context_strings);
                }

                string_tree_t *post_context_tree = NULL;
                string_tree_iterator_t *post_context_iter = NULL;

                cstring_array *post_context_strings = NULL;

                if (post_context_type != CONTEXT_TYPE_NONE) {
                    post_context_strings = cstring_array_new();
                }

                if (post_context_type == CONTEXT_TYPE_REGEX) {
                    log_debug("post_context_type == CONTEXT_TYPE_REGEX\n");
                    post_context_tree = regex_string_tree(post_context, post_context_len);

                    post_context_iter = string_tree_iterator_new(post_context_tree);

                    char_array *post_context_perm = char_array_new_size(post_context_len);

                    for (; !string_tree_iterator_done(post_context_iter); string_tree_iterator_next(post_context_iter)) {
                        char_array_clear(post_context_perm);
                        for (c = 0; c < post_context_iter->num_tokens; c++) {
                            token = string_tree_iterator_get_string(post_context_iter, c);
                            if (token == NULL) {
                                log_error ("post_token_context is NULL\n");
                            } else if (strlen(token) == 0) {
                                log_error("post_token_context is 0 length\n");
                            }
                            char_array_cat(post_context_perm, token);
                        }

                        cstring_array_add_string(post_context_strings, char_array_get_string(post_context_perm));
                    }

                    char_array_destroy(post_context_perm);
                    string_tree_iterator_destroy(post_context_iter);
                    string_tree_destroy(post_context_tree);
                } else if (post_context_type == CONTEXT_TYPE_STRING) {
                    if (post_context == NULL || strlen(post_context) == 0) {
                        log_error("post_context STRING NULL or 0 length\n");
                    }
                    cstring_array_add_string(post_context_strings, post_context);
                } else if (post_context_type == CONTEXT_TYPE_WORD_BOUNDARY) {
                    cstring_array_add_string(post_context_strings, WORD_BOUNDARY_CHAR);
                }

                size_t num_post_context_strings = 0;
                if (post_context_type != CONTEXT_TYPE_NONE) {
                    num_post_context_strings = cstring_array_num_strings(post_context_strings);
                    log_debug("num_post_context_strings = %zu\n", num_post_context_strings);
                }

                cstring_array *context_strings = NULL;
                size_t num_context_strings = 0;
                char *context_start_char = NULL;
                bool combined_context_strings = false;

                int ante, post;

                if (num_pre_context_strings > 0 && num_post_context_strings > 0) {
                    context_start_char = PRE_CONTEXT_CHAR;
                    combined_context_strings = true;
                    size_t max_string_size = 2 * MAX_UTF8_CHAR_SIZE + 
                                             ((pre_context_max_len * MAX_UTF8_CHAR_SIZE) * 
                                             (post_context_max_len * MAX_UTF8_CHAR_SIZE));
                    num_context_strings = num_pre_context_strings * num_post_context_strings;
                    char_array *context = char_array_new_size(max_string_size);
                    context_strings = cstring_array_new_size(num_context_strings * max_string_size + num_context_strings);
                    for (ante = 0; ante < num_pre_context_strings; ante++) {
                        char_array_clear(context);

                        token = cstring_array_get_string(pre_context_strings, ante);
                        if (token == NULL || strlen(token) == 0) {
                            log_error("pre_context token was NULL or 0 length\n");
                            goto exit_teardown;
                        }

                        char_array_cat(context, token);
                        size_t context_len = strlen(char_array_get_string(context));

                        for (post = 0; post < num_post_context_strings; post++) {
                            context->n = context_len;
                            char_array_cat(context, POST_CONTEXT_CHAR);
                            token = cstring_array_get_string(post_context_strings, post);
                            char_array_cat(context, token);
                            if (token == NULL || strlen(token) == 0) {
                                log_error("post_context token was NULL or 0 length\n");
                                goto exit_teardown;
                            }

                            token = char_array_get_string(context);
                            cstring_array_add_string(context_strings, token);

                        }

                    }

                    char_array_destroy(context);

                } else if (num_pre_context_strings > 0) {
                    context_start_char = PRE_CONTEXT_CHAR;
                    num_context_strings = num_pre_context_strings;
                    context_strings = pre_context_strings;
                } else if (num_post_context_strings > 0) {
                    context_start_char = POST_CONTEXT_CHAR;
                    num_context_strings = num_post_context_strings;
                    context_strings = post_context_strings;
                }

                if (num_context_strings > 0) {
                    log_debug("num_context_strings = %zu\n", num_context_strings);
                }


                if (tree == NULL) {
                    log_error("Tree was NULL, rule=%s\n", key);
                    goto exit_teardown;
                }

                string_tree_iterator_t *iter = string_tree_iterator_new(tree);

                log_debug("iter->remaining=%d\n", iter->remaining);
                
                char *key_str;

                for (; !string_tree_iterator_done(iter); string_tree_iterator_next(iter)) {
                    rule_key->n = step_key_len;

                    for (c = 0; c < iter->num_tokens; c++) {
                        token = string_tree_iterator_get_string(iter, c);
                        if (token == NULL) {
                            log_error("string_tree_iterator_get_string was NULL: %s\n", key);
                            goto exit_teardown;
                        }
                        char_array_cat(rule_key, token);
                        log_debug("string_tree token was %s\n", token);
                    }

                    log_debug("rule_key=%s\n", char_array_get_string(rule_key));

                    size_t context_key_len;

                    if (num_context_strings == 0) {

                        token = char_array_get_string(rule_key);
                        if (trie_get(trie, token) == NULL_NODE_ID) {
                            trie_add(trie, token, replacement_index);
                        } else {
                            log_warn("Key exists: %s, skipping\n", token);                            
                        }
                    } else {
                        char_array_cat(rule_key, context_start_char);
                        context_key_len = strlen(char_array_get_string(rule_key));

                        for (c = 0; c < num_context_strings; c++) {
                            rule_key->n = context_key_len;
                            token = cstring_array_get_string(context_strings, c);
                            if (token == NULL) {
                                log_error("token was NULL for c=%d\n", c);
                            }
                            char_array_cat(rule_key, token);
                            token = char_array_get_string(rule_key);
                            if (trie_get(trie, token) == NULL_NODE_ID) {
                                trie_add(trie, token, replacement_index);
                            } else {
                                log_warn("Key exists: %s, skipping\n", token);
                            }
                        }

                    }

                }

                string_tree_iterator_destroy(iter);
                string_tree_destroy(tree);

                char_array_destroy(rule_key);

                if (pre_context_strings != NULL) {
                    cstring_array_destroy(pre_context_strings);
                }

                if (post_context_strings != NULL) {
                    cstring_array_destroy(post_context_strings);
                }

                // Only needed if we created a combined context array
                if (combined_context_strings) {
                    cstring_array_destroy(context_strings);
                }
            }

            char_array_destroy(step_key);

        }

        char_array_destroy(trans_key);

        if (!transliteration_table_add_transliterator(trans)) {
            goto exit_teardown;
        }

    }

    size_t num_source_scripts = sizeof(script_transliteration_rules) / sizeof(script_transliteration_rule_t);

    for (int i = 0; i < num_source_scripts; i++) {
        script_transliteration_rule_t rule = script_transliteration_rules[i];

        if (!transliteration_table_add_script_language(rule.script_language, rule.index)) {
            goto exit_teardown;
        }

        transliterator_index_t index = rule.index;

        for (int j = index.transliterator_index; j < index.transliterator_index + index.num_transliterators; j++) {
            char *trans_name = script_transliterators[j];
            if (trans_name == NULL) {
                goto exit_teardown;
            }
            cstring_array_add_string(trans_table->transliterator_names, trans_name);
        }

    }

    transliteration_table_write(f);
    fclose(f);
    transliteration_module_teardown();
    log_info("Done!\n");
    exit(EXIT_SUCCESS);

exit_teardown:
    log_error("FAIL\n");
    transliteration_module_teardown();
    exit(EXIT_FAILURE);

}