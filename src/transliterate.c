#include <math.h>
#include "transliterate.h"
#include "file_utils.h"

#include "log/log.h"
#include "strndup.h"

#define TRANSLITERATION_TABLE_SIGNATURE 0xAAAAAAAA

#define NFD "NFD"
#define NFC "NFC"
#define NFKC "NFKC"
#define NFKD "NFKD"
#define STRIP_MARK "STRIP_MARK"

static transliteration_table_t *trans_table = NULL;

transliteration_table_t *get_transliteration_table(void) {
    return trans_table;
}

transliterator_t *transliterator_new(char *name, uint8_t internal, uint32_t steps_index, size_t steps_length) {
    transliterator_t *trans = malloc(sizeof(transliterator_t));

    if (trans == NULL) {
        return NULL;
    }

    trans->name = name;
    trans->internal = internal;
    trans->steps_index = steps_index;
    trans->steps_length = steps_length;

    return trans;
}

void transliterator_destroy(transliterator_t *self) {
    if (self == NULL) return;
    if (self->name) {
        free(self->name);
    }
    free(self);
}


transliterator_t *get_transliterator(char *name) {
    if (trans_table == NULL) {
        return NULL;
    }

    khiter_t k;
    k = kh_get(str_transliterator, trans_table->transliterators, name);
    return (k != kh_end(trans_table->transliterators)) ? kh_value(trans_table->transliterators, k) : NULL;
}


typedef enum {
    TRANS_STATE_BEGIN,
    TRANS_STATE_PARTIAL_MATCH,
    TRANS_STATE_MATCH
} transliteration_state_type_t;

typedef struct {
    trie_prefix_result_t result;
    transliteration_state_type_t state;
    ssize_t phrase_start;
    size_t phrase_len;
    size_t char_len;
    uint8_t advance_index:1;
    uint8_t advance_state:1;
    uint8_t in_set:1;
    uint8_t empty_transition:1;
    uint8_t repeat:1;
    uint8_t word_boundary:1;
} transliteration_state_t;


#define TRANSLITERATION_DEFAULT_STATE (transliteration_state_t){NULL_PREFIX_RESULT, TRANS_STATE_BEGIN, 0, 0, 0, 1, 1, 0, 0, 0, 0}


static transliteration_replacement_t *get_replacement(trie_t *trie, trie_prefix_result_t result) {
    uint32_t node_id = result.node_id;
    if (node_id == NULL_NODE_ID) return NULL;

    uint32_t replacement_index = 0;

    if (!trie_get_data_at_index(trie, node_id, &replacement_index)) {
        return NULL;
    }

    if (replacement_index < trans_table->replacements->n) {
        log_debug("Got data node\n");
        return trans_table->replacements->a[replacement_index];
    }

    return NULL;

}

typedef enum {
    NO_CHAR_RESULT,
    SINGLE_CHAR_ONLY,
    SINGLE_CHAR_REPEAT,
    OPEN_CHAR_SET,
    CLOSED_CHAR_SET,
    CHAR_SET_REPEAT,
    SINGLE_EMPTY_TRANSITION,
    CHAR_SET_EMPTY_TRANSITION
} char_set_type;


typedef struct char_set_result {
    trie_prefix_result_t result;
    char_set_type type;
} char_set_result_t;

#define NULL_CHAR_SET_RESULT (char_set_result_t){NULL_PREFIX_RESULT, NO_CHAR_RESULT};

static char_set_result_t next_prefix_or_set(trie_t *trie, char *str, size_t len, trie_prefix_result_t last_result, bool in_set, bool check_set_only) {
    trie_prefix_result_t result = trie_get_prefix_from_index(trie, str, len, last_result.node_id, last_result.tail_pos);

    bool has_empty_transition = false;


    if (!check_set_only && result.node_id != NULL_NODE_ID) {
        last_result = result;
        result = trie_get_prefix_from_index(trie, REPEAT_CHAR, REPEAT_CHAR_LEN, last_result.node_id, last_result.tail_pos);
        if (result.node_id == NULL_NODE_ID) {
            return (char_set_result_t){last_result, SINGLE_CHAR_ONLY};
        } else {
            log_debug("Got single char repeat\n");
            return (char_set_result_t){last_result, SINGLE_CHAR_REPEAT};
        }
    } else if (!in_set) {
        result = trie_get_prefix_from_index(trie, BEGIN_SET_CHAR, BEGIN_SET_CHAR_LEN, last_result.node_id, last_result.tail_pos);

        if (result.node_id == NULL_NODE_ID) {
            result = trie_get_prefix_from_index(trie, EMPTY_TRANSITION_CHAR, EMPTY_TRANSITION_CHAR_LEN, last_result.node_id, last_result.tail_pos);
            if (result.node_id == NULL_NODE_ID) {
                return NULL_CHAR_SET_RESULT;
            } else {
                log_debug("empty result node_id=%d\n", result.node_id);
                return (char_set_result_t){result, SINGLE_EMPTY_TRANSITION};
            }
        }

        log_debug("Got begin set, node_id = %d\n", result.node_id);

        last_result = result;

        result = trie_get_prefix_from_index(trie, str, len, last_result.node_id, last_result.tail_pos);

        log_debug("Set node_id = %d, len=%zu\n", result.node_id, len);

        if (result.node_id == NULL_NODE_ID) {
            result = trie_get_prefix_from_index(trie, EMPTY_TRANSITION_CHAR, EMPTY_TRANSITION_CHAR_LEN, last_result.node_id, last_result.tail_pos);
            if (result.node_id == NULL_NODE_ID) {
                return NULL_CHAR_SET_RESULT;
            }
            log_debug("Got empty transition char\n");
            has_empty_transition = true;
        }

        in_set = true;
        last_result = result; 
    }

    if (in_set) {
        // In the set but can potentially have more than one unicode character
        result = trie_get_prefix_from_index(trie, END_SET_CHAR, END_SET_CHAR_LEN, last_result.node_id, last_result.tail_pos);
        if (result.node_id == NULL_NODE_ID && !has_empty_transition) {
            log_debug("No end set\n");
            return (char_set_result_t){last_result, OPEN_CHAR_SET};
        } else if (result.node_id == NULL_NODE_ID && has_empty_transition) {
            log_debug("has_empty_transition\n");
            return NULL_CHAR_SET_RESULT;
        }

        last_result = result;
        result = trie_get_prefix_from_index(trie, REPEAT_CHAR, REPEAT_CHAR_LEN, last_result.node_id, last_result.tail_pos);

        if (result.node_id == NULL_NODE_ID && !has_empty_transition) {
            log_debug("Got closed set\n");
            return (char_set_result_t){last_result, CLOSED_CHAR_SET};
        // Shouldn't repeat the empty transition, so ignore repeats
        } else if (has_empty_transition) {
            log_debug("Char set empty transition\n");
            return (char_set_result_t){result, CHAR_SET_EMPTY_TRANSITION};
        } else {
            log_debug("Char set repeated\n");
            return (char_set_result_t){result, CHAR_SET_REPEAT};
        }
    }
    return NULL_CHAR_SET_RESULT;

}

static transliteration_state_t state_from_char_result(char_set_result_t char_result, size_t index, size_t len, transliteration_state_t prev_state, bool is_context) {
    transliteration_state_t state = TRANSLITERATION_DEFAULT_STATE;

    trie_prefix_result_t result = char_result.result;

    state.result = result;
    state.char_len = len;
    state.in_set = (char_result.type == OPEN_CHAR_SET || (prev_state.in_set && char_result.type == SINGLE_CHAR_ONLY));
    state.repeat = (char_result.type == SINGLE_CHAR_REPEAT || char_result.type == CHAR_SET_REPEAT);
    state.empty_transition = (char_result.type == SINGLE_EMPTY_TRANSITION || char_result.type == CHAR_SET_EMPTY_TRANSITION);

    if (char_result.type != NO_CHAR_RESULT) {
        log_debug("in state_from_char_result, char_result.type = %d\n", char_result.type);
        state.state = TRANS_STATE_PARTIAL_MATCH;

        if (!is_context) {
            if (prev_state.state == TRANS_STATE_BEGIN) {
                state.phrase_start = index;
            } else {
                state.phrase_start = prev_state.phrase_start;
            }
            state.phrase_len = prev_state.phrase_len + len;
        }
    }

    return state;

}

static transliteration_state_t state_transition(trie_t *trie, char *str, size_t index, size_t len, transliteration_state_t prev_state) {

    log_debug("str = %s, index = %zu, char_len=%zu\n", str, index, len);

    log_debug("prev_state.result.node_id=%d, prev_state.in_set=%d\n", prev_state.result.node_id, prev_state.in_set);

    char_set_result_t char_result = next_prefix_or_set(trie, str + index, len, prev_state.result, prev_state.in_set, false);

    log_debug("char_result.type = %d\n", char_result.type);

    return state_from_char_result(char_result, index, len, prev_state, false);
}


static transliteration_state_t state_transition_context(trie_t *trie, char *str, size_t index, size_t len, transliteration_state_t prev_state) {

    log_debug("str = %s, index = %zu, char_len=%zu\n", str, index, len);

    log_debug("prev_state.result.node_id=%d, prev_state.in_set=%d\n", prev_state.result.node_id, prev_state.in_set);

    char_set_result_t char_result = next_prefix_or_set(trie, str + index, len, prev_state.result, prev_state.in_set, false);

    log_debug("char_result.type = %d\n", char_result.type);

    return state_from_char_result(char_result, index, len, prev_state, true);
}



static inline void set_match_if_any(trie_t *trie, transliteration_state_t state, transliteration_state_t *match_state) {
    if (state.state != TRANS_STATE_PARTIAL_MATCH) return;

    trie_prefix_result_t prev_result = state.result;

    // Complete string
    trie_prefix_result_t result = trie_get_prefix_from_index(trie, "", 1, prev_result.node_id, prev_result.tail_pos);
    if (result.node_id != NULL_NODE_ID) {
        match_state->result = result;
        match_state->state = TRANS_STATE_MATCH;
        match_state->phrase_start = state.phrase_start;
        match_state->phrase_len = state.phrase_len;
    }
}


static transliteration_state_t check_pre_context(trie_t *trie, char *str, transliteration_state_t original_state) {
    size_t start_index = original_state.phrase_start;
    int32_t ch = 0;
    size_t idx = start_index;
    ssize_t char_len = 0;

    bool in_repeat = false;

    transliteration_state_t prev_state = original_state;
    transliteration_state_t state = original_state;

    // Save the end of the repeated state the first time through
    transliteration_state_t repeat_state_end;

    transliteration_state_t match_state = TRANSLITERATION_DEFAULT_STATE;

    log_debug("start_index=%zu, str=%s\n", start_index, str);

    while (idx > 0) {
        char_len = utf8proc_iterate_reversed((uint8_t *)str, idx, &ch);

        if (char_len <= 0) {
            break;
        }

        if (!utf8proc_codepoint_valid(ch)) {
            idx -= char_len;
            continue;
        }

        log_debug("In pre-context, got char %d, \"%.*s\"\n", ch, (int)char_len, str + idx - char_len);

        state = state_transition_context(trie, str, idx - char_len, char_len, prev_state);        
        set_match_if_any(trie, state, &match_state);

        if (match_state.state == TRANS_STATE_MATCH) {
            log_debug("pre-context TRANS_STATE_MATCH\n");
            state = match_state;
            break;
        } else if (state.state == TRANS_STATE_BEGIN && !in_repeat) {
            log_debug("pre-context TRANS_STATE_BEGIN and not in repeat\n");
            if (prev_state.state == TRANS_STATE_PARTIAL_MATCH) {
                state = prev_state;
            }
            break;
        } else if (state.repeat) {
            log_debug("pre-context in repeat\n");
            in_repeat = true;
            repeat_state_end = state;
            state.advance_state = false;    
        } else if (state.empty_transition) {
            log_debug("pre-context empty_transition\n");
            state.advance_index = false;
            if (in_repeat) {
                log_debug("empty_transition in repeat\n");
                prev_state = repeat_state_end;
                state.advance_state = false;
                in_repeat = false;
            }
        // If we're repeating e.g. "[abcd]+e", when we hit the "e" or another character, stop repeating and try from the end of the block
        } else if (state.state == TRANS_STATE_BEGIN && in_repeat) {
            log_debug("pre-context stop repeat\n");
            prev_state = repeat_state_end;
            in_repeat = false;
            state.advance_index = false;
            state.advance_state = false;
        } else if (in_repeat) {
            log_debug("end repeat\n");
            log_debug("state.state==%d, state.result.node_id=%d, repeat_state_end.result.node_id=%d\n", state.state, state.result.node_id, repeat_state_end.result.node_id);
            in_repeat = false;
            break;
        }

        if (state.advance_index) {
            idx -= char_len;
        }

        if (state.advance_state) {
            prev_state = state;
        }
        
    }

    return state;
}

static transliteration_state_t check_post_context(trie_t *trie, char *str, transliteration_state_t original_state) {
    size_t index = original_state.phrase_start + original_state.phrase_len;
    uint8_t *ptr = (uint8_t *)str + index;
    size_t len = strlen(str) - index;
    int32_t ch = 0;
    size_t idx = 0;
    ssize_t char_len = 0;

    bool in_repeat = false;

    transliteration_state_t prev_state = original_state;

    transliteration_state_t state = original_state;

    // Save the end of the repeated state the first time through
    transliteration_state_t repeat_state_end;

    transliteration_state_t match_state = TRANSLITERATION_DEFAULT_STATE;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &ch);

        if (char_len <= 0) {
            break;
        }

        if (!utf8proc_codepoint_valid(ch)) {
            idx += char_len;
            ptr += char_len;
            continue;
        }

        log_debug("In post-context, got char \"%.*s\"\n", (int)char_len, str + index + idx);

        state = state_transition_context(trie, str, index + idx, char_len, prev_state);
        set_match_if_any(trie, state, &match_state);

        if (match_state.state == TRANS_STATE_MATCH) {
            log_debug("post-context TRANS_STATE_MATCH\n");
            state = match_state;
            break;
        } else if (state.state == TRANS_STATE_BEGIN && !in_repeat) {
            log_debug("post-context TRANS_STATE_BEGIN and not in repeat\n");
            break;
        } else if (state.repeat) {
            log_debug("post-context in repeat\n");
            in_repeat = true;
            repeat_state_end = state;
            state.advance_state = false;    
        } else if (state.empty_transition) {
            log_debug("post-context empty_transition\n");
            state.advance_index = false;
            if (in_repeat) {
                log_debug("empty_transition in repeat\n");
                prev_state = repeat_state_end;
                state.advance_state = false;
                in_repeat = false;
            }
        // If we're repeating e.g. "[abcd]+e", when we hit the "e" or another character, stop repeating and try from the end of the block
        } else if (state.state == TRANS_STATE_BEGIN && in_repeat) {
            log_debug("post-context stop repeat\n");
            prev_state = repeat_state_end;
            in_repeat = false;
            state.advance_index = false;
            state.advance_state = false;
        } else if (in_repeat) {
            log_debug("end repeat\n");
            in_repeat = false;
            break;
        }

        if (state.advance_index) {
            idx += char_len;
            ptr += char_len;
        }

        if (state.advance_state) {
            prev_state = state;
        }
        
    }

    return state;
}

static trie_prefix_result_t context_match(trie_t *trie, char *str, transliteration_state_t original_state) {
    trie_prefix_result_t prev_result = original_state.result;
    transliteration_state_t state = TRANSLITERATION_DEFAULT_STATE;
    transliteration_state_t prev_state = original_state;
    trie_prefix_result_t result = trie_get_prefix_from_index(trie, PRE_CONTEXT_CHAR, PRE_CONTEXT_CHAR_LEN, prev_result.node_id, prev_result.tail_pos);

    log_debug("phrase_start=%zd, phrase_len=%zu\n", original_state.phrase_start, original_state.phrase_len);

    if (result.node_id != NULL_NODE_ID) {
        prev_state.result = result;
        log_debug("Have pre_context\n");
        state = check_pre_context(trie, str, prev_state);
        
        if (state.state == TRANS_STATE_MATCH && state.result.node_id != prev_state.result.node_id) {
            return state.result;
        }

        if (state.state == TRANS_STATE_PARTIAL_MATCH && state.result.node_id != prev_state.result.node_id) {
            log_debug("Pre-context partial match\n");
        }

        prev_result = state.result;
        prev_state = state;
    }

    result = trie_get_prefix_from_index(trie, POST_CONTEXT_CHAR, POST_CONTEXT_CHAR_LEN, prev_result.node_id, prev_result.tail_pos);        
    if (result.node_id != NULL_NODE_ID) {
        prev_state.result = result;
        log_debug("Have post_context\n");
        state = check_post_context(trie, str, prev_state);
        if (state.state == TRANS_STATE_MATCH && state.result.node_id != prev_state.result.node_id) {
            return state.result;
        }
    }

    log_debug("Failed to match context\n");
    return NULL_PREFIX_RESULT;
}

static char *replace_groups(trie_t *trie, char *str, char *replacement, group_capture_array *groups, transliteration_state_t original_state) {
    size_t idx = 0;

    int32_t ch = 0;
    ssize_t char_len = 0;
    uint8_t *ptr = (uint8_t *)str + original_state.phrase_start;

    log_debug("str=%s\n", (char *)ptr);

    size_t len = original_state.phrase_len;

    log_debug("phrase_start = %zd, phrase_len = %zu\n", original_state.phrase_start, original_state.phrase_len);

    size_t num_groups = groups->n;

    log_debug("num_groups = %zu\n", num_groups);

    if (num_groups == 0) {
        return NULL;
    }

    cstring_array *group_strings = cstring_array_new_size(num_groups);

    log_debug("Created arrays\n");

    transliteration_state_t state = original_state;
    transliteration_state_t prev_state = original_state;

    transliteration_state_t repeat_state_end = TRANSLITERATION_DEFAULT_STATE;

    size_t group_num = 0;
    group_capture_t group = groups->a[group_num];

    log_debug("group = {%zu, %zu}\n", group.start, group.len);

    bool in_group = false;
    bool in_repeat = false;

    size_t group_start = 0;
    size_t group_len = 0;

    log_debug("group now {%zu, %zu}\n", group_start, group_len);

    size_t num_chars = 0;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &ch);

        log_debug("Got char '%.*s' at idx=%zu, len=%zu\n", (int)char_len, ptr, idx, char_len);

        if (char_len <= 0) { 
            break;
        }

        if (!(utf8proc_codepoint_valid(ch))) {
            log_warn("Invalid codepoint: %d\n", ch);
            continue;
        }

        state = state_transition(trie, str, idx, char_len, prev_state);

        if (state.state == TRANS_STATE_BEGIN && !in_repeat) {
            log_debug("Normal char: %.*s\n", (int)char_len, ptr);
            prev_state = original_state;
        } else if (state.repeat) {
            log_debug("state.repeat\n");
            in_repeat = true;
            repeat_state_end = state;
            state.advance_state = false;
        } else if (state.empty_transition) {
            log_debug("state.empty_transition\n");
            state.advance_index = false;
            num_chars++;
        } else if (state.state == TRANS_STATE_BEGIN && in_repeat && state.result.node_id == repeat_state_end.result.node_id) {
            log_debug("end of repeat\n");
            prev_state = repeat_state_end;
            state.advance_index = false;
            state.advance_state = false;
        } else if (in_repeat) {
            log_debug("in repeat\n");
            in_repeat = false;
            state.advance_index = false;
            state.advance_state = false;
        }

        if (state.advance_index) {
            if (num_chars == group.start) {
                log_debug("Starting group\n");
                in_group = true;
                group_start = idx;
                log_debug("group_start = %zu\n", group_start);
            }

            if (in_group) {
                log_debug("In group\n");
                group_len += char_len;
                
                log_debug("group_len=%zu\n", group_len);
                log_debug("group.start + group.len = %zu\n", group.start + group.len);
                if (num_chars == group.start + group.len - 1) {
                    in_group = false;
                    log_debug("adding group str %.*s\n", (int)group_len, str + original_state.phrase_start + group_start);
                    cstring_array_add_string_len(group_strings, str + original_state.phrase_start + group_start, group_len);
                    if (group_num < num_groups - 1) {
                        group_num++;
                        log_debug("group_num=%zu\n", group_num);
                        group = groups->a[group_num];
                        group_len = 0;
                    }
                    state = TRANSLITERATION_DEFAULT_STATE;
                }
            }

        }

        if (state.advance_index) {
            ptr += char_len;
            idx += char_len;
            num_chars++;
            log_debug("num_chars = %zu\n", num_chars);
        }

        if (state.advance_state) {
            prev_state = state;
        }

    }

    bool in_group_ref = false;

    int group_ref = 0;

    size_t group_num_start = 0;
    size_t group_num_len = 0;

    idx = 0;

    log_debug("Doing replacements\n");

    size_t replacement_len = strlen(replacement);

    log_debug("replacement = %s, len = %zu\n", replacement, replacement_len);

    char_array *ret = char_array_new_size(replacement_len);

    uint8_t *replacement_ptr = (uint8_t *)replacement;

    while (idx < replacement_len) {
        char_len = utf8proc_iterate(replacement_ptr, replacement_len, &ch);

        if (ch == GROUP_INDICATOR_CODEPOINT) {
            log_debug("start group ref\n");
            in_group_ref = true;
            group_num_start = idx + 1;
            group_num_len = 0;
            idx += char_len;
            replacement_ptr += char_len;
            continue;
        } else if (in_group_ref) {
            log_debug("in group ref\n");
            sscanf((char *)replacement_ptr, "%d", &group_ref);
            log_debug("Got group_ref=%d\n", group_ref);
            char *group = cstring_array_get_string(group_strings, group_ref-1);
            log_debug("Got group=%s\n", group);
            if (group != NULL) {
                char_array_cat(ret, group);
            }
            log_debug("Did cat\n");
            if (group_ref > 0) {
                size_t group_ref_len = (int)(log10(group_ref) + 1);
                log_debug("group_ref_len=%zu\n", group_ref_len);
                idx += group_ref_len;
                replacement_ptr += group_ref_len;
            }
            in_group_ref = false;
        } else {
            log_debug("ptr=%.*s\n", (int)char_len, replacement_ptr);
            char_array_cat_len(ret, (char *)replacement_ptr, char_len);
            idx += char_len;
            replacement_ptr += char_len;
        }
    }

    cstring_array_destroy(group_strings);
    return char_array_to_string(ret);
}

char *transliterate(char *trans_name, char *str, size_t len) {
    if (trans_name == NULL || str == NULL) return NULL;

    transliteration_table_t *trans_table = get_transliteration_table();

    if (trans_table == NULL) {
        log_error("transliteration table is NULL. Call libpostal_setup() or transliteration_module_setup()\n");
        return NULL;
    }

    trie_t *trie = trans_table->trie;

    if (trie == NULL) {
        log_warn("transliteration table not initialized\n");
        return NULL;
    }

    log_debug("len = %zu\n", len);

    str = strndup(str, len);

    bool allocated_trans_name = false;

    if (!string_is_lower(trans_name)) {
        trans_name = strdup(trans_name);

        // Transliterator names are ASCII strings, so this is fine
        string_lower(trans_name);
        allocated_trans_name = true;
    }

    log_debug("lower = %s\n", trans_name);

    transliterator_t *transliterator = get_transliterator(trans_name);
    if (transliterator == NULL) {
        log_warn("transliterator \"%s\" does not exist\n", trans_name);
        if (allocated_trans_name) free(trans_name);
        free(str);
        return NULL;
    }

    log_debug("got transliterator\n");

    trie_prefix_result_t result = trie_get_prefix(trie, trans_name);

    log_debug("result = {%d, %zu}\n", result.node_id, result.tail_pos);

    uint32_t trans_node_id = result.node_id;

    if (allocated_trans_name) free(trans_name);

    result = trie_get_prefix_from_index(trans_table->trie, NAMESPACE_SEPARATOR_CHAR, NAMESPACE_SEPARATOR_CHAR_LEN, result.node_id, result.tail_pos);

    trans_node_id = result.node_id;

    trie_prefix_result_t trans_result = result;

    log_debug("trans_node_id = %d\n", trans_node_id);

    transliteration_step_t *step;
    char *step_name;

    char_array *new_str = NULL;

    for (uint32_t i = transliterator->steps_index; i < transliterator->steps_index + transliterator->steps_length; i++) {
        step = trans_table->steps->a[i];
        step_name = step->name;
        if (step->type == STEP_RULESET && trans_node_id == NULL_NODE_ID) {
            log_warn("transliterator \"%s\" does not exist in trie\n", trans_name);
            free(str);
            return NULL;
        }

        if (step->type == STEP_RULESET) {
            log_debug("ruleset\n");
            result = trie_get_prefix_from_index(trie, step_name, strlen(step_name), trans_result.node_id, trans_result.tail_pos);
            uint32_t step_node_id = result.node_id;

            if (step_node_id == NULL_NODE_ID) {
                log_warn("transliterator step \"%s\" does not exist\n", step_name);
                free(str);
                return NULL;
            }

            result = trie_get_prefix_from_index(trie, NAMESPACE_SEPARATOR_CHAR, NAMESPACE_SEPARATOR_CHAR_LEN, result.node_id, result.tail_pos);
            step_node_id = result.node_id;

            log_debug("step_node_id = %d\n", step_node_id);

            trie_prefix_result_t step_result = result;
            trie_prefix_result_t context_result = NULL_PREFIX_RESULT;

            new_str = char_array_new_size(len);

            transliteration_state_t state = TRANSLITERATION_DEFAULT_STATE;

            transliteration_state_t start_state = TRANSLITERATION_DEFAULT_STATE;
            start_state.result = step_result;

            transliteration_state_t prev_state = start_state;
            transliteration_state_t prev2_state = start_state;

            transliteration_state_t repeat_state_end = start_state;

            bool in_repeat = false;

            int32_t ch = 0;
            ssize_t char_len = 0;
            uint8_t *ptr = (uint8_t *)str;
            size_t idx = 0;
            
            char *original_str = str;
            char_array *revisit = NULL;

            transliteration_replacement_t *replacement = NULL;

            transliteration_state_t match_state = TRANSLITERATION_DEFAULT_STATE;

            while (idx < len) {
                log_debug("idx=%zu, ptr=%s\n", idx, ptr);
                char_len = utf8proc_iterate(ptr, len, &ch);
                if (char_len == UTF8PROC_ERROR_INVALIDUTF8) {
                    log_warn("invalid UTF-8\n");
                    char_len = 1;
                    ch = (int32_t)*ptr;
                } else if (char_len <= 0) {
                    log_warn("char_len=%zd at idx=%zu\n", char_len, idx);
                    free(trans_name);
                    free(str);
                    return NULL;
                }

                if (!(utf8proc_codepoint_valid(ch))) {
                    log_warn("Invalid codepoint: %d\n", ch);
                    idx += char_len;
                    ptr += char_len;
                    continue;
                }

                if (ch == 0) break;

                log_debug("Got char '%.*s' at idx=%zu, prev_state.state=%d\n", (int)char_len, str + idx, idx, prev_state.state);

                state = state_transition(trie, str, idx, char_len, prev_state);
                set_match_if_any(trie, state, &match_state);

                replacement = NULL;

                if ((state.state == TRANS_STATE_BEGIN && prev_state.state == TRANS_STATE_PARTIAL_MATCH) ||
                    (state.state == TRANS_STATE_PARTIAL_MATCH && idx + char_len == len)) {

                    log_debug("end of partial or last char, prev start=%zd, prev len=%zu\n", prev_state.phrase_start, prev_state.phrase_len);

                    bool context_no_match = false;

                    bool is_last_char = idx + char_len == len;
                    
                    transliteration_state_t match_candidate_state = state.state == TRANS_STATE_PARTIAL_MATCH ? state : prev_state;
                    if (state.state == TRANS_STATE_PARTIAL_MATCH) {
                        log_debug("state.state == TRANS_STATE_PARTIAL_MATCH\n");
                    }

                    context_result = context_match(trie, str, match_candidate_state);

                    if (context_result.node_id != NULL_NODE_ID) {
                        log_debug("Context match\n");
                        match_state = match_candidate_state;
                        match_state.state = TRANS_STATE_MATCH;
                        replacement = get_replacement(trie, context_result);
                    } else {
                        if (match_state.state == TRANS_STATE_MATCH) { 
                            log_debug("Context no match and previous match\n");
                            replacement = get_replacement(trie, match_state.result);
                            if (state.state != TRANS_STATE_PARTIAL_MATCH) {
                                state.advance_index = false;
                            }
                        } else {
                            log_debug("Checking for no-context match\n");
                            set_match_if_any(trie, match_candidate_state, &match_state);
                            if (match_state.state != TRANS_STATE_MATCH && !match_candidate_state.in_set) {
                                log_debug("Trying set for match candidate\n");

                                transliteration_state_t match_prev_state = !is_last_char ? prev2_state : prev_state;

                                log_debug("idx = %zu, match_candidate_state.char_len = %zu\n", idx, match_candidate_state.char_len);

                                char_set_result_t char_result = next_prefix_or_set(trie, str + idx, match_candidate_state.char_len, match_prev_state.result, false, true);
                                log_debug("char_result.type = %d\n", char_result.type);
                                bool is_context = false;

                                match_candidate_state = state_from_char_result(char_result, idx, match_candidate_state.char_len, match_prev_state, is_context);
                                if (match_candidate_state.state == TRANS_STATE_PARTIAL_MATCH) {
                                    log_debug("Got partial match for set check\n");
                                    set_match_if_any(trie, match_candidate_state, &match_state);
                                    if (match_state.state != TRANS_STATE_MATCH && !match_candidate_state.empty_transition) {
                                        log_debug("match_state.state != TRANS_STATE_MATCH && !match_candidate_state.empty_transition\n");
                                        prev_state = match_candidate_state;
                                    }
                                }
                            }

                            if (match_state.state == TRANS_STATE_MATCH) {
                                log_debug("Match no context\n");
                                replacement = get_replacement(trie, match_state.result);
                            } else {

                                log_debug("Tried context for %s at char '%.*s', no match\n", str, (int)char_len, ptr);
                                context_no_match = true;
                            }
                        }

                    }

                    if (replacement != NULL) {
                        char *replacement_string = cstring_array_get_string(trans_table->replacement_strings, replacement->string_index);
                        char *revisit_string = NULL;
                        if (replacement->revisit_index != 0) {
                            log_debug("revisit_index = %d\n", replacement->revisit_index);
                            revisit_string = cstring_array_get_string(trans_table->revisit_strings, replacement->revisit_index);
                        }

                        bool free_revisit = false;
                        bool free_replacement = false;

                        if (replacement->groups != NULL) {
                            log_debug("Did groups, str=%s\n", str);
                            replacement_string = replace_groups(trie, str, replacement_string, replacement->groups, match_state);
                            free_replacement = (replacement_string != NULL);
                            if (revisit_string != NULL) {
                                log_debug("===Doing revisit\n");
                                revisit_string = replace_groups(trie, str, revisit_string, replacement->groups, match_state);
                                free_revisit = (revisit_string != NULL);
                            }
                        }

                        if (revisit_string != NULL) {
                            log_debug("revisit_string not null, %s\n", revisit_string);
                            size_t revisit_size = strlen(revisit_string) + len - idx;
                            if (revisit == NULL) {
                                revisit = char_array_new_size(revisit_size + 1);
                            } else {
                                log_debug("revisit not null\n");
                                char_array_clear(revisit);
                            }

                            char_array_cat(revisit, revisit_string);
                            char_array_cat_len(revisit, str + idx, len - idx);
                            
                            idx = 0;
                            len = revisit_size;
                            str = char_array_get_string(revisit);
                            ptr = (uint8_t *)str;
                            log_debug("Switching to revisit=%s, size=%zu\n", str, revisit_size);
                        }

                        char_array_cat(new_str, replacement_string);
                        log_debug("Replacement = %s, revisit = %s\n", replacement_string, revisit_string);

                        if (free_replacement) {
                            free(replacement_string);
                        }
                        if (free_revisit) {
                            free(revisit_string);
                        }

                        match_state = TRANSLITERATION_DEFAULT_STATE;
                    }

                    bool added_previous_phrase = false;

                    if (context_no_match && !prev_state.empty_transition && prev_state.phrase_len > 0) {
                        log_debug("Previous phrase stays as is %.*s\n", (int)prev_state.phrase_len, str+prev_state.phrase_start);
                        char_array_cat_len(new_str, str + prev_state.phrase_start, prev_state.phrase_len);
                        added_previous_phrase = true;

                        if (match_candidate_state.state != TRANS_STATE_PARTIAL_MATCH) {
                            state = start_state;
                        }

                    }

                    if (match_candidate_state.state != TRANS_STATE_PARTIAL_MATCH && !prev_state.empty_transition && idx + char_len == len) {
                        log_debug("No replacement for %.*s\n", (int)char_len, ptr);
                        char_array_cat_len(new_str, str + idx, char_len);
                        state = start_state;
                    } else if (state.state == TRANS_STATE_BEGIN && !prev_state.empty_transition) {
                        log_debug("TRANS_STATE_BEGIN && !prev_state.empty_transition\n");
                        state.advance_index = false;
                    } else if (prev_state.empty_transition) {
                        log_debug("No replacement for %.*s\n", (int)char_len, ptr);
                        char_array_cat_len(new_str, str + idx, char_len);
                    }

                    state.advance_state = false;
                    prev_state = start_state;
                } else if (state.state == TRANS_STATE_BEGIN && !in_repeat) {
                    log_debug("No replacement for %.*s\n", (int)char_len, ptr);
                    char_array_cat_len(new_str, str + idx, char_len);
                    prev_state = start_state;
                    state.advance_state = false;
                } else if (state.repeat) {
                    log_debug("state.repeat\n");
                    in_repeat = true;
                    repeat_state_end = state;
                    state.advance_state = false;
                } else if (state.empty_transition) {
                    log_debug("state.empty_transition\n");
                    state.advance_index = false;
                } else if (state.state == TRANS_STATE_BEGIN && in_repeat && state.result.node_id == repeat_state_end.result.node_id) {
                    prev_state = repeat_state_end;
                    state.advance_index = false;
                    state.advance_state = false;
                } else if (in_repeat) {
                    in_repeat = false;
                    state.advance_index = false;
                    state.advance_state = false;
                }
                
                log_debug("state.phrase_start = %zd, state.phrase_len=%zu\n", state.phrase_start, state.phrase_len);
                if (state.advance_index) {
                    ptr += char_len;
                    idx += char_len;
                }

                if (state.advance_state) {
                    prev2_state = prev_state;
                    prev_state = state;
                }

            }

            if (revisit != NULL) {
                char_array_destroy(revisit);
            }

            log_debug("original_str=%s\n", original_str);

            free(original_str);

            str = char_array_to_string(new_str);

            log_debug("new_str = %s\n", str);

        } else if (step->type == STEP_UNICODE_NORMALIZATION) {
            log_debug("unicode normalization\n");
            int utf8proc_options = UTF8PROC_OPTIONS_BASE;
            if (string_equals(step->name, NFD)) {
                utf8proc_options = UTF8PROC_OPTIONS_NFD;
            } else if (string_equals(step->name, NFC)) {
                utf8proc_options = UTF8PROC_OPTIONS_NFC;
            } else if (string_equals(step->name, NFKD)) {
                utf8proc_options = UTF8PROC_OPTIONS_NFKD;
            } else if (string_equals(step->name, NFKC)) {
                utf8proc_options = UTF8PROC_OPTIONS_NFKC;
            } else if (string_equals(step->name, STRIP_MARK)) {
                utf8proc_options = UTF8PROC_OPTIONS_STRIP_ACCENTS;
            }

            uint8_t *utf8proc_normalized = NULL;
            utf8proc_map((uint8_t *)str, 0, &utf8proc_normalized, utf8proc_options);
            if (utf8proc_normalized != NULL) {
                char *old_str = str;
                str = (char *)utf8proc_normalized;
                log_debug("utf8proc_normalized=%s\n", utf8proc_normalized);
                len = strlen(str);
                free(old_str);
            }
            log_debug("Got unicode normalization step, new str=%s, len=%lu\n", str, strlen(str));
        } else if (step->type == STEP_TRANSFORM) {
            // Recursive call here shouldn't hurt too much, happens in only a few languages and only 2-3 calls deep
            log_debug("Got STEP_TYPE_TRANSFORM, step=%s\n", step_name);
            char *old_str = str;
            str = transliterate(step_name, str, strlen(str));
            log_debug("Transform result = %s\n", str);
            log_debug("str = %s\n", str);
            len = strlen(str);
            free(old_str);
        }

    }

    return str;

}

void transliteration_table_destroy(void) {
    transliteration_table_t *trans_table = get_transliteration_table();
    if (trans_table == NULL) return;
    if (trans_table->trie) {
        trie_destroy(trans_table->trie);
    }

    if (trans_table->transliterators) {
        transliterator_t *trans;
        kh_foreach_value(trans_table->transliterators, trans, {
            transliterator_destroy(trans);
        })

        kh_destroy(str_transliterator, trans_table->transliterators);
    }

    if (trans_table->script_languages) {
        kh_destroy(script_language_index, trans_table->script_languages);
    }

    if (trans_table->transliterator_names) {
        cstring_array_destroy(trans_table->transliterator_names);
    }

    if (trans_table->steps) {
        step_array_destroy(trans_table->steps);
    }

    if (trans_table->replacements) {
        transliteration_replacement_array_destroy(trans_table->replacements);
    }

    if (trans_table->replacement_strings) {
        cstring_array_destroy(trans_table->replacement_strings);
    }

    if (trans_table->revisit_strings) {
        cstring_array_destroy(trans_table->revisit_strings);
    }

    free(trans_table);
}


transliteration_table_t *transliteration_table_init(void) {
    transliteration_table_t *trans_table = get_transliteration_table();

    if (trans_table == NULL) {
        trans_table = calloc(1, sizeof(transliteration_table_t));

        trans_table->trie = trie_new();
        if (trans_table->trie == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->transliterators = kh_init(str_transliterator);
        if (trans_table->transliterators == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->script_languages = kh_init(script_language_index);
        if (trans_table->script_languages == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->transliterator_names = cstring_array_new();
        if (trans_table->transliterator_names == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->steps = step_array_new();
        if (trans_table->steps == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->replacements = transliteration_replacement_array_new();
        if (trans_table->replacements == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->replacement_strings = cstring_array_new();
        if (trans_table->replacement_strings == NULL) {
            goto exit_trans_table_created;
        }

        trans_table->revisit_strings = cstring_array_new();
        if (trans_table->revisit_strings == NULL) {
            goto exit_trans_table_created;
        }

    }

    return trans_table;

exit_trans_table_created:
   transliteration_table_destroy();
   exit(1);
}

transliteration_table_t *transliteration_table_new(void) {
    transliteration_table_t *trans_table = transliteration_table_init();
    if (trans_table != NULL) {
        cstring_array_add_string(trans_table->replacement_strings, "");
        cstring_array_add_string(trans_table->revisit_strings, "");
    }
    return trans_table;
}

transliteration_step_t *transliteration_step_new(char *name, step_type_t type) {
    transliteration_step_t *self = malloc(sizeof(transliteration_step_t));

    if (self == NULL) {
        return NULL;
    }

    self->name = strdup(name);
    if (self->name == NULL) {
        transliteration_step_destroy(self);
    }

    self->type = type;
    return self;
}


void transliteration_step_destroy(transliteration_step_t *self) {
    if (self == NULL) {
        return;
    }

    if (self->name != NULL) {
        free(self->name);
    }

    free(self);
}


transliteration_replacement_t *transliteration_replacement_new(uint32_t string_index, uint32_t revisit_index, group_capture_array *groups) {
    transliteration_replacement_t *replacement = malloc(sizeof(transliteration_replacement_t)); 

    if (replacement == NULL) {
        return NULL;
    }

    replacement->num_groups = groups == NULL ? 0 : groups->n;
    replacement->groups = groups;

    replacement->string_index = string_index;
    replacement->revisit_index = revisit_index;
    return replacement;

}

void transliteration_replacement_destroy(transliteration_replacement_t *self) {
    if (self == NULL) return;

    if (self->groups != NULL) {
        group_capture_array_destroy(self->groups);
    }

    free(self);
}

bool transliteration_table_add_transliterator(transliterator_t *trans) {
    if (trans_table == NULL) {
        return false;
    }

    int ret;
    khiter_t k = kh_put(str_transliterator, trans_table->transliterators, trans->name, &ret);
    if (ret < 0) return false;
    kh_value(trans_table->transliterators, k) = trans;

    return true;
}

bool transliteration_table_add_script_language(script_language_t script_language, transliterator_index_t index) {
    if (trans_table == NULL) {
        return false;
    }

    int ret;
    khiter_t k = kh_put(script_language_index, trans_table->script_languages, script_language, &ret);
    if (ret < 0) return false;
    kh_value(trans_table->script_languages, k) = index;

    return true;
}

transliterator_index_t get_transliterator_index_for_script_language(script_t script, char *language) {
    if (trans_table == NULL || language == NULL || strlen(language) >= MAX_LANGUAGE_LEN) {
        return NULL_TRANSLITERATOR_INDEX;
    }

    script_language_t script_lang;
    script_lang.script = script;
    strcpy(script_lang.language, language);

    khiter_t k;
    k = kh_get(script_language_index, trans_table->script_languages, script_lang);
    return (k != kh_end(trans_table->script_languages)) ? kh_value(trans_table->script_languages, k) : NULL_TRANSLITERATOR_INDEX;
}


char *transliterator_replace_strings(trie_t *trie, cstring_array *replacements, char *input) {
    phrase_array *phrases;
    char_array *str;
    char *current = input;
    bool is_original = true;
    
    size_t len = strlen(input);

    // We may go through several rounds of replacements
    while (1) {
        phrases = trie_search(trie, current);
        if (!phrases) {
            break;
        } else {
            str = char_array_new_size(len);
            phrase_t phrase;
            size_t start = 0;
            size_t end = 0;
            for (size_t i = 0; i < phrases->n; i++) {
                phrase = phrases->a[i];
                end = phrase.start;
                char_array_append_len(str, input + start, end - start);
                char_array_append(str, cstring_array_get_string(replacements, phrase.data));
                start = phrase.start + phrase.len;
            }

            char_array_append_len(str, input + end, len - end);
            char_array_terminate(str);

            if (!is_original) {
                free(current);
            }

            // Destroys the char array itself, but not the string it holds
            current = char_array_to_string(str);
            is_original = false;
        }
    }

    return current;
}

transliterator_t *transliterator_read(FILE *f) {
    uint64_t trans_name_len;

    if (!file_read_uint64(f, &trans_name_len)) {
        return NULL;
    }

    char *name = malloc(trans_name_len);
    if (name == NULL) {
        return NULL;
    }

    if (!file_read_chars(f, name, trans_name_len)) {
        return NULL;
    }

    bool internal;
    if (!file_read_uint8(f, (uint8_t *)&internal)) {
        return NULL;
    }

    uint32_t steps_index;

    if (!file_read_uint32(f, &steps_index)) {
        return NULL;
    }


    uint32_t steps_length;

    if (!file_read_uint32(f, &steps_length)) {
        return NULL;
    }

    transliterator_t *trans =  transliterator_new(name, internal, steps_index, steps_length);
    return trans;
}

bool transliterator_write(transliterator_t *trans, FILE *f) {
    size_t trans_name_len = strlen(trans->name) + 1;
    if (!file_write_uint64(f, (uint64_t)trans_name_len) || 
        !file_write_chars(f, trans->name, trans_name_len)) {
        return false;
    }

    if (!file_write_uint8(f, trans->internal)) {
        return false;
    }

    if (!file_write_uint32(f, trans->steps_index)) {
        return false;
    }

    if (!file_write_uint32(f, (uint32_t)trans->steps_length)) {
        return false;
    }

    return true;
}

transliteration_step_t *transliteration_step_read(FILE *f) {
    uint64_t step_name_len;

    log_debug("reading step\n");;

    transliteration_step_t *step = malloc(sizeof(transliteration_step_t));
    if (step == NULL) {
        return NULL;
    }

    if (!file_read_uint32(f, &step->type)) {
        goto exit_step_destroy;
    }
    if (!file_read_uint64(f, &step_name_len)) {
        goto exit_step_destroy;
    }

    char *name = malloc(step_name_len);
    if (name == NULL) {
        goto exit_step_destroy;
    }

    if (!file_read_chars(f, name, step_name_len)) {
        free(name);
        goto exit_step_destroy;
    }
    step->name = name;

    return step;

exit_step_destroy:
    free(step);
    return NULL;
}

bool transliteration_step_write(transliteration_step_t *step, FILE *f) {
    if (!file_write_uint32(f, step->type)) {
        return false;
    }

    // Include the NUL byte
    size_t step_name_len = strlen(step->name) + 1;

    if (!file_write_uint64(f, (uint64_t)step_name_len) || 
        !file_write_chars(f, step->name, step_name_len)) {
        return false;
    }

    return true;
}

bool group_capture_read(FILE *f, group_capture_t *group) {
    uint64_t start;
    if (!file_read_uint64(f, &start)) {
        return false;
    }
    group->start = (size_t)start;

    uint64_t len;
    if (!file_read_uint64(f, &len)) {
        return false;
    }

    group->len = (size_t)len;

    return true;
}

bool group_capture_write(group_capture_t group, FILE *f) {
    if (!file_write_uint64(f, (uint64_t)group.start) ||
        !file_write_uint64(f, (uint64_t)group.len)) {
        return false;
    }

    return true;
}

transliteration_replacement_t *transliteration_replacement_read(FILE *f) {
    uint32_t string_index;

    if (!file_read_uint32(f, &string_index)) {
        return NULL;
    }

    uint32_t revisit_index;

    if (!file_read_uint32(f, &revisit_index)) {
        return NULL;
    }

    uint64_t num_groups;

    if (!file_read_uint64(f, &num_groups)) {
        return NULL;
    }

    group_capture_array *groups = NULL;

    if (num_groups > 0) {
        groups = group_capture_array_new_size((size_t)num_groups);
        group_capture_t group;
        for (size_t i = 0; i < (size_t)num_groups; i++) {
            if (!group_capture_read(f, &group)) {
                group_capture_array_destroy(groups);
                return NULL;
            }
            group_capture_array_push(groups, group);
        }

    }


    return transliteration_replacement_new(string_index, revisit_index, groups);
}

bool transliteration_replacement_write(transliteration_replacement_t *replacement, FILE *f) {
    if (!file_write_uint32(f, replacement->string_index)) {
        return false;
    }

    if (!file_write_uint32(f, replacement->revisit_index)) {
        return false;
    }

    if (!file_write_uint64(f, replacement->num_groups)) {
        return false;
    }

    group_capture_t group;

    for (size_t i = 0; i < replacement->num_groups; i++) {
        group = replacement->groups->a[i];
        if (!group_capture_write(group, f)) {
            return false;
        }
    }

    return true;

}

bool transliteration_table_read(FILE *f) {
    if (f == NULL) {
        return false;
    }

    uint32_t signature;

    log_debug("Reading signature\n");

    if (!file_read_uint32(f, &signature) || signature != TRANSLITERATION_TABLE_SIGNATURE) {
        return false;
    }

    trans_table = transliteration_table_init();

    log_debug("Table initialized\n");

    uint64_t num_transliterators = 0;

    if (!file_read_uint64(f, &num_transliterators)) {
        goto exit_trans_table_load_error;
    }


    log_debug("num_transliterators = %zu\n", (size_t)num_transliterators);

    size_t i;

    transliterator_t *trans;

    for (i = 0; i < (size_t)num_transliterators; i++) {
        trans = transliterator_read(f);
        if (trans == NULL) {
            log_error("trans was NULL\n");
            goto exit_trans_table_load_error;
        } else {
            log_debug("read trans with name: %s\n", trans->name);
        }
        if (!transliteration_table_add_transliterator(trans)) {
            goto exit_trans_table_load_error;
        }
    }

    log_debug("Read transliterators\n");

    uint64_t num_script_languages;
    if (!file_read_uint64(f, &num_script_languages)) {
        goto exit_trans_table_load_error;
    }

    log_debug("num_script_languages = %zu\n", (size_t)num_script_languages);

    script_language_t script_language;
    transliterator_index_t index;

    uint64_t language_len = 0;
    char language[MAX_LANGUAGE_LEN] = "";

    uint64_t transliterator_index = 0;
    uint64_t index_num_transliterators = 0;

    for (i = 0; i < num_script_languages; i++) {
        if (!file_read_uint32(f, (uint32_t *)&script_language.script)) {
            goto exit_trans_table_load_error;
        }

        if (!file_read_uint64(f, &language_len) || language_len >= MAX_LANGUAGE_LEN) {
            goto exit_trans_table_load_error;
        }

        if (language_len == 0) {
            script_language.language[0] = '\0';
        } else if (!file_read_chars(f, (char *)language, (size_t)language_len)) {
            goto exit_trans_table_load_error;
        } else {
            strcpy(script_language.language, language);
        }
        
        if (!file_read_uint64(f, &transliterator_index)) {
            goto exit_trans_table_load_error;
        }

        index.transliterator_index = (size_t)transliterator_index;

        if (!file_read_uint64(f, &index_num_transliterators)) {
            goto exit_trans_table_load_error;
        }

        index.num_transliterators = (size_t)index_num_transliterators;

        log_debug("Adding script language key={%d, %s}, value={%zu, %zu}\n", script_language.script, script_language.language, index.transliterator_index, index.num_transliterators);

        transliteration_table_add_script_language(script_language, index);
    }

    uint64_t trans_table_num_strings;

    if (!file_read_uint64(f, &trans_table_num_strings)) {
        goto exit_trans_table_load_error;
    }

    log_debug("trans_table_num_strings=%zu\n", (size_t)trans_table_num_strings);

    uint64_t trans_name_str_len;

    if (!file_read_uint64(f, &trans_name_str_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("Creating char_array with size=%zu\n", (size_t)trans_name_str_len);

    char_array *array = char_array_new_size((size_t)trans_name_str_len);

    if (!file_read_chars(f, array->a, (size_t)trans_name_str_len)) {
        goto exit_trans_table_load_error;
    }

    array->n = trans_name_str_len;

    cstring_array_destroy(trans_table->transliterator_names);
    log_debug("Destroyed current cstring_array\n");

    log_debug("char_array len=%zu\n", array->n);

    trans_table->transliterator_names = cstring_array_from_char_array(array);
    log_debug("Set trans_table->transliterator_names\n");

    if (cstring_array_num_strings(trans_table->transliterator_names) != trans_table_num_strings) {
        goto exit_trans_table_load_error;
    }

    uint64_t num_steps;

    if (!file_read_uint64(f, &num_steps)) {
        goto exit_trans_table_load_error;
    }

    log_debug("num_steps = %zu\n", (size_t)num_steps);

    if (!step_array_resize(trans_table->steps, (size_t)num_steps)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    transliteration_step_t *step;

    for (i = 0; i < num_steps; i++) {
        step = transliteration_step_read(f);
        if (step == NULL) {
            goto exit_trans_table_load_error;
        }
        log_debug("Read step with name %s and type %d\n", step->name, step->type);
        step_array_push(trans_table->steps, step);
    }

    log_debug("Done with steps\n");

    transliteration_replacement_t *replacement;

    uint64_t num_replacements;

    if (!file_read_uint64(f, &num_replacements)) {
        goto exit_trans_table_load_error;
    }

    log_debug("num_replacements = %zu\n", (size_t)num_replacements);

    if (!transliteration_replacement_array_resize(trans_table->replacements, (size_t)num_replacements)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    for (i = 0; i < num_replacements; i++) {
        replacement = transliteration_replacement_read(f);
        if (replacement == NULL) {
            goto exit_trans_table_load_error;
        }
        transliteration_replacement_array_push(trans_table->replacements, replacement);
    }

    log_debug("Done with replacements\n");

    uint64_t num_replacement_tokens;

    if (!file_read_uint64(f, &num_replacement_tokens)) {
        goto exit_trans_table_load_error;
    }

    log_debug("num_replacement_tokens = %zu\n", (size_t)num_replacement_tokens);

    if (!uint32_array_resize(trans_table->replacement_strings->indices, (size_t)num_replacement_tokens)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    uint32_t token_index;

    for (i = 0; i < num_replacement_tokens; i++) {
        if (!file_read_uint32(f, &token_index)) {
            goto exit_trans_table_load_error;
        }
        uint32_array_push(trans_table->replacement_strings->indices, token_index);
    }

    log_debug("Done with replacement token indices\n");

    uint64_t replacement_strings_len;

    if (!file_read_uint64(f, &replacement_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("replacement_strings_len = %zu\n", (size_t)replacement_strings_len);

    if (!char_array_resize(trans_table->replacement_strings->str, (size_t)replacement_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    if (!file_read_chars(f, trans_table->replacement_strings->str->a, (size_t)replacement_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("Read replacement_strings\n");

    trans_table->replacement_strings->str->n = replacement_strings_len;

    uint64_t num_revisit_tokens;

    if (!file_read_uint64(f, &num_revisit_tokens)) {
        goto exit_trans_table_load_error;
    }

    log_debug("num_revisit_tokens = %zu\n", (size_t)num_revisit_tokens);

    if (!uint32_array_resize(trans_table->revisit_strings->indices, (size_t)num_revisit_tokens)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    for (i = 0; i < num_revisit_tokens; i++) {
        if (!file_read_uint32(f, &token_index)) {
            goto exit_trans_table_load_error;
        }
        uint32_array_push(trans_table->revisit_strings->indices, token_index);
    }

    log_debug("Done with revisit token indices\n");

    uint64_t revisit_strings_len = 0;

    if (!file_read_uint64(f, &revisit_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("revisit_strings_len = %zu\n", (size_t)revisit_strings_len);

    if (!char_array_resize(trans_table->revisit_strings->str, (size_t)revisit_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("resized\n");

    if (!file_read_chars(f, trans_table->revisit_strings->str->a, (size_t)revisit_strings_len)) {
        goto exit_trans_table_load_error;
    }

    log_debug("Read revisit_strings\n");

    trans_table->revisit_strings->str->n = revisit_strings_len;

    // Free the default trie
    trie_destroy(trans_table->trie);

    trans_table->trie = trie_read(f);
    log_debug("Read trie\n");
    if (trans_table->trie == NULL) {
        goto exit_trans_table_load_error;
    }

    return true;

exit_trans_table_load_error:
    transliteration_table_destroy();
    return false;
}

bool transliteration_table_write(FILE *f) {
    if (f == NULL) {
        return false;
    }

    transliterator_t *trans;

    if (!file_write_uint32(f, TRANSLITERATION_TABLE_SIGNATURE)) {
        return false;
    }

    size_t num_transliterators = kh_size(trans_table->transliterators);

    if (!file_write_uint64(f, (uint64_t)num_transliterators)) {
        return false;
    }

    kh_foreach_value(trans_table->transliterators, trans, {
        if (!transliterator_write(trans, f)) {
            return false;
        }
    })

    size_t i;

    size_t num_script_languages = kh_size(trans_table->script_languages);

    if (!file_write_uint64(f, (uint64_t)num_script_languages)) {
        return false;
    }

    script_language_t script_language;
    transliterator_index_t index;

    kh_foreach(trans_table->script_languages, script_language, index, {
        if (!file_write_uint32(f, (uint32_t)script_language.script)) {
            return false;
        }

        size_t language_len = strlen(script_language.language);

        if (!file_write_uint64(f, (uint64_t)language_len)) {
            return false;
        }

        if (language_len > 0 && !file_write_chars(f, script_language.language, language_len)) {
            return false;
        }

        if (!file_write_uint64(f, (uint64_t)index.transliterator_index)) {
            return false;
        }

        if (!file_write_uint64(f, (uint64_t)index.num_transliterators)) {
            return false;
        }
    })

    size_t num_trans_names = trans_table->transliterator_names->indices->n;

    if (!file_write_uint64(f, (uint64_t)num_trans_names)) {
        return false;
    }

    size_t trans_names_str_len = trans_table->transliterator_names->str->n;

    if (!file_write_uint64(f, (uint64_t)trans_names_str_len)) {
        return false;
    }

    if (!file_write_chars(f, trans_table->transliterator_names->str->a, trans_names_str_len)) {
        return false;
    }

    transliteration_step_t *step;


    size_t num_steps = trans_table->steps->n;

    if (!file_write_uint64(f, num_steps)) {
        return false;
    }

    for (i = 0; i < num_steps; i++) {
        step = trans_table->steps->a[i];
        if (!transliteration_step_write(step, f)) {
            return false;
        }
    }

    size_t num_replacements = trans_table->replacements->n;

    if (!file_write_uint64(f, num_replacements)) {
        return false;
    }

    transliteration_replacement_t *replacement;

    for (i = 0; i < trans_table->replacements->n; i++) {
        replacement = trans_table->replacements->a[i];
        if (!transliteration_replacement_write(replacement, f)) {
            return false;
        }
    }

    size_t replacement_tokens_len = trans_table->replacement_strings->indices->n;

    if (!file_write_uint64(f, replacement_tokens_len)) {
        return false;
    }

    for (i = 0; i < replacement_tokens_len; i++) {
        if (!file_write_uint32(f, trans_table->replacement_strings->indices->a[i])) {
            return false;
        }
    }

    size_t replacement_strings_len = trans_table->replacement_strings->str->n;

    if (!file_write_uint64(f, replacement_strings_len)) {
        return false;
    }

    if (!file_write_chars(f, trans_table->replacement_strings->str->a, replacement_strings_len)) {
        return false;
    }

    size_t revisit_tokens_len = trans_table->revisit_strings->indices->n;

    log_debug("revisit_tokens_len=%zu\n", revisit_tokens_len);

    if (!file_write_uint64(f, revisit_tokens_len)) {
        return false;
    }

    for (i = 0; i < revisit_tokens_len; i++) {
        if (!file_write_uint32(f, trans_table->revisit_strings->indices->a[i])) {
            return false;
        }
    }

    size_t revisit_strings_len = trans_table->revisit_strings->str->n;

    if (!file_write_uint64(f, revisit_strings_len)) {
        return false;
    }

    if (!file_write_chars(f, trans_table->revisit_strings->str->a, revisit_strings_len)) {
        return false;
    }

    if (!trie_write(trans_table->trie, f)) {
        return false;
    }

    return true;

}

bool transliteration_table_load(char *filename) {
    if (filename == NULL || trans_table != NULL) {
        return false;
    }

    FILE *f;

    if ((f = fopen(filename, "rb")) != NULL) {
        bool ret = transliteration_table_read(f);
        fclose(f);
        return ret;
    } else {
        return false;
    }
}


bool transliteration_table_save(char *filename) {
    if (trans_table == NULL || filename == NULL) {
        return false;
    }

    FILE *f;

    if ((f = fopen(filename, "wb")) != NULL) {
        bool ret = transliteration_table_write(f);
        fclose(f);
        return ret;
    } else {
        return false;
    }

}

bool transliteration_module_init(void) {
    trans_table = transliteration_table_new();
    return trans_table != NULL;
}

bool transliteration_module_setup(char *filename) {
    if (trans_table == NULL) {
        return transliteration_table_load(filename == NULL ? DEFAULT_TRANSLITERATION_PATH : filename);
    }

    return true;
}


void transliteration_module_teardown(void) {
    transliteration_table_destroy();
    trans_table = NULL;
}

