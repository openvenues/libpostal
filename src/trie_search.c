#include "trie_search.h"

typedef enum {
    SEARCH_STATE_BEGIN,
    SEARCH_STATE_NO_MATCH,
    SEARCH_STATE_PARTIAL_MATCH,
    SEARCH_STATE_MATCH
} trie_search_state_t;

phrase_array *trie_search(trie_t *self, char *text) {
    if (text == NULL) return NULL;

    phrase_array *phrases = phrase_array_new();

    ssize_t len, remaining;
    int32_t unich = 0;
    unsigned char ch = '\0';

    const uint8_t *ptr = (const uint8_t *)text;
    const uint8_t *fail_ptr = ptr;
    trie_node_t node = trie_get_root(self), last_node = node;

    uint32_t node_id = ROOT_NODE_ID;
    uint32_t next_id;

    bool match = false;
    uint64_t index = 0;
    int phrase_len = 0, phrase_start = 0;
    uint32_t data;

    trie_search_state_t state = SEARCH_STATE_BEGIN, last_state = SEARCH_STATE_BEGIN;

    bool advance_index = true;

    while(1) {
        len = utf8proc_iterate(ptr, -1, &unich);
        remaining = len;
        if (len <= 0) return NULL;
        if (!(utf8proc_codepoint_valid(unich))) return NULL;

        bool is_letter = utf8_is_letter(unich);

        // If we're in the middle of a word and the first letter was not a match, skip the word
        if (is_letter && state == SEARCH_STATE_NO_MATCH) { 
            log_debug("skipping\n", NULL);
            ptr += len;
            index += len;
            last_state = state;
            continue; 
        }

        // Match in the middle of a word
        if (is_letter && last_state == SEARCH_STATE_MATCH) {
            log_debug("last_state == SEARCH_STATE_MATCH && is_letter\n", NULL);
            // Only set match to false so we don't callback
            match = false;
        }

        for (int i=0; remaining > 0; remaining--, i++, ptr++, last_node=node, last_state=state, node_id=next_id) {
            ch = (unsigned char) *ptr;
            log_debug("char=%c\n", ch);

            next_id = trie_get_transition_index(self, node, *ptr);
            node = trie_get_node(self, next_id);

            if (node.check != node_id) {
                state = is_letter ? SEARCH_STATE_NO_MATCH : SEARCH_STATE_BEGIN;
                if (match) {
                    log_debug("match is true and state==SEARCH_STATE_NO_MATCH\n", NULL);
                    phrase_array_push(phrases, (phrase_t){phrase_start, phrase_len, data});
                    index = phrase_start + phrase_len;
                    advance_index = false;
                    // Set the text back to the end of the last phrase
                    ptr = (const uint8_t *)text + index;
                } else {
                    ptr += remaining;
                    log_debug("done with char, now at %s\n", ptr);
                }
                fail_ptr = ptr;
                last_node = node = trie_get_root(self);
                node_id = ROOT_NODE_ID;
                phrase_start = phrase_len = 0;
                last_state = state;
                match = false;
                break;
            } else {
                log_debug("node.check == node_id\n", NULL);
                state = SEARCH_STATE_PARTIAL_MATCH;
                if (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN) {
                    log_debug("phrase_start=%llu\n", index);
                    phrase_start = index;
                    fail_ptr = ptr + remaining;
                }

                if (node.base < 0) {
                    int32_t data_index = -1*node.base;
                    trie_data_node_t data_node = self->data->a[data_index];
                    unsigned char *current_tail = self->tail->a + data_node.tail;
                    data = data_node.data;

                    size_t tail_len = strlen((char *)current_tail);
                    char *query_tail = (char *)(*ptr ? ptr + 1 : ptr);
                    size_t query_tail_len = strlen((char *)query_tail);
                    log_debug("next node tail: %s vs %s\n", current_tail, query_tail);

                    if (tail_len <= query_tail_len && strncmp((char *)current_tail, query_tail, tail_len) == 0) {
                        state = SEARCH_STATE_MATCH;
                        log_debug("Tail matches\n", NULL);
                        last_state = state;
                        data = data_node.data;
                        log_debug("%llu, %d, %zu\n", index, phrase_len, tail_len);
                        ptr += tail_len;
                        index += tail_len;
                        advance_index = false;
                        phrase_len = index + len - phrase_start;
                        match = true;
                    } else if (match) {
                        log_debug("match is true and longer phrase tail did not match\n", NULL);
                        log_debug("phrase_start=%d, phrase_len=%d\n", phrase_start, phrase_len);
                        phrase_array_push(phrases, (phrase_t){phrase_start, phrase_len, data});
                        ptr = fail_ptr;
                        match = false;
                        index = phrase_start + phrase_len;
                        advance_index = false;
                    }

                } 

                if (ch != '\0') {
                    trie_node_t terminal_node = trie_get_transition(self, node, '\0');
                    if (terminal_node.check == next_id) {
                        log_debug("Transition to NUL byte matched\n", NULL);
                        state = SEARCH_STATE_MATCH;
                        match = true;
                        phrase_len = index + len - phrase_start;
                        if (terminal_node.base < 0) {
                            int32_t data_index = -1*terminal_node.base;
                            trie_data_node_t data_node = self->data->a[data_index];
                            data = data_node.data;
                        }
                        log_debug("Got match with len=%d\n", phrase_len);
                        fail_ptr = ptr;
                    }
                }
            }

        }

        if (unich == 0) {
            if (last_state == SEARCH_STATE_MATCH) {
                log_debug("Found match at the end\n", NULL);
                phrase_array_push(phrases, (phrase_t){phrase_start, phrase_len, data});
            }
            break;
        }

        if (advance_index) index += len;

        advance_index = true;
        log_debug("index now %llu\n", index);
    } // while

    return phrases;
}

int trie_node_search_tail_tokens(trie_t *self, trie_node_t node, char *str, token_array *tokens, int tail_index, int token_index) {
    int32_t data_index = -1*node.base;
    trie_data_node_t old_data_node = self->data->a[data_index];
    uint32_t current_tail_pos = old_data_node.tail;

    unsigned char *tail_ptr = self->tail->a + current_tail_pos + tail_index;
    
    if (!(*tail_ptr)) {
        log_debug("tail matches!\n", NULL);
        return token_index-1;
    }

    log_debug("Searching tail: %s\n", tail_ptr);
    for (int i = token_index; i < tokens->n; i++) {
        token_t token = tokens->a[i];

        char *ptr = str + token.offset;
        int token_length = token.len;

        if (!(*tail_ptr)) {
            log_debug("tail matches!\n", NULL);
            return i-1;
        }

        if (i < tokens->n - 1 && *tail_ptr == ' ') {
            tail_ptr++;
        }

        log_debug("Tail string compare: %s with %.*s\n", tail_ptr, (int)token_length, ptr);

        if (strncmp((char *)tail_ptr, ptr, token_length) == 0) {
            tail_ptr += token_length;
        } else {
            return -1;
        }
    }
    return -1;

}

phrase_array *trie_search_tokens(trie_t *self, char *str, token_array *tokens) {
    if (str == NULL || tokens == NULL || tokens->n == 0) return NULL;

    phrase_array *phrases = phrase_array_new();

    trie_node_t node = trie_get_root(self), last_node = node;
    uint32_t node_id = ROOT_NODE_ID, last_node_id = ROOT_NODE_ID;

    uint32_t data;

    int phrase_len = 0, phrase_start = 0, last_match_index = -1;

    trie_search_state_t state = SEARCH_STATE_BEGIN, last_state = SEARCH_STATE_BEGIN;

    token_t token;
    size_t token_length, token_consumed;

    log_debug("num_tokens: %zu\n", tokens->n);
    for (int i = 0; i < tokens->n; i++, last_state = state) {
        token = tokens->a[i];
        token_length = token.len;
        
        char *ptr = str + token.offset;
        log_debug("On %d, token=%.*s\n", i, (int)token_length, ptr);

        for (int j = 0; j < token_length; j++, ptr++, last_node = node, last_node_id = node_id) {
            log_debug("Getting transition index for %d, (%d, %d)\n", node_id, node.base, node.check);
            if (j > 0 || last_node.base >= 0) {
                node_id = trie_get_transition_index(self, node, *ptr);
                node = trie_get_node(self, node_id);
                log_debug("Doing %c, got node_id=%d\n", *ptr, node_id);
            } else {
                log_debug("Tail stored on space node, rolling back one character\n");
                ptr--;
            }

            //if (last_node.check && last_node->tail) { node = last_node; node_id = last_node_id; }

            if (node.check != last_node_id && last_node.base >= 0) {
                log_debug("Fell off trie. last_node_id=%d and node.check=%d\n", last_node_id, node.check);
                node = trie_get_root(self);
                node_id = ROOT_NODE_ID;
                break;
            } else if (node.base < 0) {
                log_debug("Searching tail at index %d\n", i);

                uint32_t data_index = -1*node.base;
                trie_data_node_t data_node = self->data->a[data_index];
                uint32_t current_tail_pos = data_node.tail;
                data = data_node.data;

                unsigned char *current_tail = self->tail->a + current_tail_pos;

                log_debug("next node tail: %s vs %.*s\n", current_tail, (int)token_length - j, ptr + 1);
                size_t ptr_len = token_length - j;

                if (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN) {
                    log_debug("phrase start at %d\n", i);
                    phrase_start = i;
                }
                if (strncmp((char *)current_tail, ptr + 1, ptr_len) == 0) {
                    log_debug("node tail matches first token\n");
                    int tail_search_result = trie_node_search_tail_tokens(self, node, str, tokens, ptr_len, i+1);
                    if (tail_search_result == -1) {
                        node = trie_get_root(self);
                        node_id = ROOT_NODE_ID;
                        break;
                    } else {
                        phrase_len = tail_search_result - phrase_start + 1;
                        last_match_index = i = tail_search_result;
                        last_state = SEARCH_STATE_MATCH;
                        break;
                    }

                } else {
                    node = trie_get_root(self);
                    node_id = ROOT_NODE_ID;
                    break;
                }
            }
        }

        if (node.check <= 0) {
            state = SEARCH_STATE_NO_MATCH;
            // check
            if (last_match_index != -1) {
                log_debug("last_match not NULL and state==SEARCH_STATE_NO_MATCH, data=%d", data);
                phrase_array_push(phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
                i = last_match_index;
                last_match_index = -1;
                phrase_start = 0;
                last_node = trie_get_root(self);
                last_node_id = ROOT_NODE_ID;
                continue;
            } else if (last_state == SEARCH_STATE_PARTIAL_MATCH) {
                log_debug("last_state == SEARCH_STATE_PARTIAL_MATCH\n");
                i = phrase_start;
                continue;
            } else {
                phrase_start = phrase_len = 0;
                // this token was not a phrase
                log_debug("Plain token=%.*s\n", token.len, str + token.offset);
            }
            last_node = trie_get_root(self);
            last_node_id = ROOT_NODE_ID;
        } else {

            state = SEARCH_STATE_PARTIAL_MATCH;
            if (!(node.base < 0) && (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN)) {
                log_debug("phrase_start=%d\n", i);
                phrase_start = i;
            }

            trie_node_t terminal_node = trie_get_transition(self, node, '\0');
            if (terminal_node.check == node_id) {
                log_debug("node match at %d\n", i);
                state = SEARCH_STATE_MATCH;
                int32_t data_index = -1*terminal_node.base;
                trie_data_node_t data_node = self->data->a[data_index];
                data = data_node.data;
                log_debug("data = %d\n", data);

                last_match_index = i;
            }

            if (i == tokens->n - 1) {
                log_debug("At last token\n");
                break;
            }

            // Check continuation
            uint32_t continuation_id = trie_get_transition_index(self, node, ' ');
            log_debug("transition_id: %d\n", continuation_id);
            trie_node_t continuation = trie_get_node(self, continuation_id);
            if (continuation.check != node_id && last_match_index != i) {
                log_debug("No continuation for phrase with start=%d, yielding tokens\n", phrase_start);
                state = SEARCH_STATE_NO_MATCH;
                phrase_start = 0;
            } else if (continuation.check != node_id && last_match_index == i) {
                log_debug("node->match no continuation\n");
                phrase_array_push(phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
                last_match_index = -1; 
                last_node = node = trie_get_root(self);
                last_node_id = node_id = ROOT_NODE_ID;
                state = SEARCH_STATE_BEGIN;
            } else {
                log_debug("Has continuation, node_id=%d\n", continuation_id);
                last_node = node = continuation;
                last_node_id = node_id = continuation_id;
            }            
        }

    }

    if (last_match_index != -1) {
        phrase_array_push(phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
   }

    return phrases;
}

phrase_t trie_search_suffixes(trie_t *self, char *word) {
    uint32_t node_id = ROOT_NODE_ID, last_node_id = ROOT_NODE_ID;
    trie_node_t last_node = trie_get_root(self);
    node_id = trie_get_transition_index(self, last_node, '\0');
    trie_node_t node = trie_get_node(self, node_id);

    if (node.check != ROOT_NODE_ID) {
        return (phrase_t){0, 0, 0};
    } else {
        last_node = node;
        last_node_id = node_id;
    }

    uint32_t value = 0, phrase_start = 0, phrase_len = 0;

    ssize_t char_len;
    size_t len = strlen(word);

    int32_t unich = 0;

    ssize_t index = len;
    const uint8_t *ptr = (const uint8_t *)word + len;
    const uint8_t *char_ptr;

    bool done = false;
    bool in_tail = false;
    unsigned char *current_tail = (unsigned char *)"";
    size_t tail_remaining = 0;

    uint32_t tail_value = 0;

    while(index > 0) {
        char_len = utf8proc_iterate_reversed(ptr, index, &unich);

        if (char_len <= 0) break;
        if (!(utf8proc_codepoint_valid(unich))) break;

        ptr -= char_len;
        index -= char_len;
        char_ptr = ptr;

        for (int i=0; i < char_len; i++, char_ptr++, last_node = node, last_node_id = node_id) {
            log_debug("char=%c\n", (unsigned char)*char_ptr);

            if (in_tail && *current_tail && *current_tail == *char_ptr) {
                tail_remaining--;
                current_tail++;
                if (i == char_len - 1) {
                    phrase_len += char_len;
                    phrase_start = index;
                }
                continue;
            } else if (in_tail && tail_remaining == 0 && i == char_len - 1) {
                log_debug("tail match!\n", NULL);
                phrase_start = index;
                phrase_len = strlen((char *)ptr);
                value = tail_value;
                done = true;
                break;
            } else if (in_tail) {
                done = true;
                log_debug("Done with tail\n", NULL);
                break;
            }

            node_id = trie_get_transition_index(self, node, *char_ptr);
            node = trie_get_node(self, node_id);
    
            if (node.check != last_node_id) { 
                log_debug("node.check = %d and last_node_id = %d\n", node.check, last_node_id);
                done = true;
                break;
            } else if (node.base < 0) {
                log_debug("Searching tail\n", NULL);

                uint32_t data_index = -1*node.base;
                trie_data_node_t data_node = self->data->a[data_index];
                uint32_t current_tail_pos = data_node.tail;

                tail_value = data_node.data;

                current_tail = self->tail->a + current_tail_pos;

                tail_remaining = strlen((char *)current_tail);
                in_tail = true;
            }
        }

        if (done) break;
    }

    if (phrase_len > 0) {
        trie_node_t terminal_node = trie_get_transition(self, node, '\0');
        if (terminal_node.check == node_id) {
            int32_t data_index = -1*terminal_node.base;
            trie_data_node_t data_node = self->data->a[data_index];
            value = data_node.data;
            log_debug("value = %d\n", value);
        }
    }

    return (phrase_t) {phrase_start, phrase_len, value};
}

phrase_t trie_search_prefixes(trie_t *self, char *word) {
    uint32_t node_id = ROOT_NODE_ID, last_node_id = node_id;
    trie_node_t node = trie_get_root(self), last_node = node;

    uint32_t value = 0, phrase_start = 0, phrase_len = 0;

    char *ptr = word;

    for (; *ptr; ptr++, phrase_len++, last_node = node, last_node_id = node_id) {
        log_debug("Getting transition index for %d, (%d, %d)\n", node_id, node.base, node.check);
        node_id = trie_get_transition_index(self, node, *ptr);
        node = trie_get_node(self, node_id);
        log_debug("Doing %c, got node_id=%d\n", *ptr, node_id);
        if (node.check != last_node_id) { 
            log_debug("node.check = %d and last_node_id = %d\n", node.check, last_node_id);
            break;
        } else if (node.base < 0) {
            log_debug("Searching tail\n", NULL);

            uint32_t data_index = -1*node.base;
            trie_data_node_t data_node = self->data->a[data_index];
            uint32_t current_tail_pos = data_node.tail;

            unsigned char *current_tail = self->tail->a + current_tail_pos;

            log_debug("comparing tail: %s vs %s\n", current_tail, ptr + 1);
            size_t current_tail_len = strlen((char *)current_tail);
            if (strncmp((char *)current_tail, ptr + 1, current_tail_len) == 0) {
                phrase_len += current_tail_len + 1;
                log_debug("tail match!\n", NULL);
                value = data_node.data;
                break;
            }
        }
    }

    if (phrase_len > 0) {
        trie_node_t terminal_node = trie_get_transition(self, node, '\0');
        if (terminal_node.check == node_id) {
            int32_t data_index = -1*terminal_node.base;
            trie_data_node_t data_node = self->data->a[data_index];
            value = data_node.data;
            log_debug("value = %d\n", value);
        }
    }

    return (phrase_t) {phrase_start, phrase_len, value};
}

