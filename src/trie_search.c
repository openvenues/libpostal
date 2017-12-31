#include "trie_search.h"

typedef enum {
    SEARCH_STATE_BEGIN,
    SEARCH_STATE_NO_MATCH,
    SEARCH_STATE_PARTIAL_MATCH,
    SEARCH_STATE_MATCH
} trie_search_state_t;

bool trie_search_from_index(trie_t *self, char *text, uint32_t start_node_id, phrase_array **phrases) {
    if (text == NULL) return false;

    ssize_t len, remaining;
    int32_t unich = 0;
    unsigned char ch = '\0';

    const uint8_t *ptr = (const uint8_t *)text;
    const uint8_t *fail_ptr = ptr;

    uint32_t node_id = start_node_id;
    trie_node_t node = trie_get_node(self, node_id), last_node = node;
    uint32_t next_id;

    bool match = false;
    uint32_t index = 0;
    uint32_t phrase_len = 0;
    uint32_t phrase_start = 0;
    uint32_t data;

    trie_search_state_t state = SEARCH_STATE_BEGIN, last_state = SEARCH_STATE_BEGIN;

    bool advance_index = true;

    while(1) {
        len = utf8proc_iterate(ptr, -1, &unich);
        remaining = len;
        if (len <= 0) return false;
        if (!(utf8proc_codepoint_valid(unich))) return false;

        int cat = utf8proc_category(unich);
        bool is_letter = utf8_is_letter(cat);

        // If we're in the middle of a word and the first letter was not a match, skip the word
        if (is_letter && state == SEARCH_STATE_NO_MATCH) { 
            log_debug("skipping\n");
            ptr += len;
            index += len;
            last_state = state;
            continue; 
        }

        // Match in the middle of a word
        if (is_letter && last_state == SEARCH_STATE_MATCH) {
            log_debug("last_state == SEARCH_STATE_MATCH && is_letter\n");
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
                    log_debug("match is true and state==SEARCH_STATE_NO_MATCH\n");
                    if (*phrases == NULL) {
                        *phrases = phrase_array_new_size(1);
                    }
                    phrase_array_push(*phrases, (phrase_t){phrase_start, phrase_len, data});
                    index = phrase_start + phrase_len;
                    advance_index = false;
                    // Set the text back to the end of the last phrase
                    ptr = (const uint8_t *)text + index;
                    len = utf8proc_iterate(ptr, -1, &unich);
                    log_debug("ptr=%s\n", ptr);
                } else {
                    ptr += remaining;
                    log_debug("done with char, now at %s\n", ptr);
                }
                fail_ptr = ptr;
                node_id = start_node_id;
                last_node = node = trie_get_node(self, node_id);
                phrase_start = phrase_len = 0;
                last_state = state;
                match = false;
                break;
            } else {
                log_debug("node.check == node_id\n");
                state = SEARCH_STATE_PARTIAL_MATCH;
                if (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN) {
                    log_debug("phrase_start=%u\n", index);
                    phrase_start = index;
                    fail_ptr = ptr + remaining;
                }

                if (node.base < 0) {
                    int32_t data_index = -1*node.base;
                    trie_data_node_t data_node = self->data->a[data_index];
                    unsigned char *current_tail = self->tail->a + data_node.tail;

                    size_t tail_len = strlen((char *)current_tail);
                    char *query_tail = (char *)(*ptr ? ptr + 1 : ptr);
                    size_t query_tail_len = strlen((char *)query_tail);
                    log_debug("next node tail: %s\n", current_tail);
                    log_debug("query node tail: %s\n", query_tail);

                    if (tail_len <= query_tail_len && strncmp((char *)current_tail, query_tail, tail_len) == 0) {
                        state = SEARCH_STATE_MATCH;
                        log_debug("Tail matches\n");
                        last_state = state;
                        data = data_node.data;
                        log_debug("%u, %d, %zu\n", index, phrase_len, tail_len);
                        ptr += tail_len;
                        index += tail_len;
                        advance_index = false;
                        phrase_len = index + 1 - phrase_start;
                        match = true;
                    } else if (match) {
                        log_debug("match is true and longer phrase tail did not match\n");
                        log_debug("phrase_start=%d, phrase_len=%d\n", phrase_start, phrase_len);
                        if (*phrases == NULL) {
                            *phrases = phrase_array_new_size(1);
                        }
                        phrase_array_push(*phrases, (phrase_t){phrase_start, phrase_len, data});
                        ptr = fail_ptr;
                        match = false;
                        index = phrase_start + phrase_len;
                        advance_index = false;
                    }

                } 

                if (ch != '\0') {
                    trie_node_t terminal_node = trie_get_transition(self, node, '\0');
                    if (terminal_node.check == next_id) {
                        log_debug("Transition to NUL byte matched\n");
                        state = SEARCH_STATE_MATCH;
                        match = true;
                        phrase_len = index + (uint32_t)len - phrase_start;
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
                log_debug("Found match at the end\n");
                if (*phrases == NULL) {
                    *phrases = phrase_array_new_size(1);
                }
                phrase_array_push(*phrases, (phrase_t){phrase_start, phrase_len, data});
            }
            break;
        }

        if (advance_index) index += len;

        advance_index = true;
        log_debug("index now %u\n", index);
    } // while

    return true;
}

inline bool trie_search_with_phrases(trie_t *self, char *str, phrase_array **phrases) {
    return trie_search_from_index(self, str, ROOT_NODE_ID, phrases);
}

inline phrase_array *trie_search(trie_t *self, char *text) {
    phrase_array *phrases = NULL;
    if (!trie_search_with_phrases(self, text, &phrases)) {
        return false;
    }
    return phrases;
}

int trie_node_search_tail_tokens(trie_t *self, trie_node_t node, char *str, token_array *tokens, size_t tail_index, int token_index) {
    int32_t data_index = -1*node.base;
    trie_data_node_t old_data_node = self->data->a[data_index];
    uint32_t current_tail_pos = old_data_node.tail;

    log_debug("tail_index = %zu\n", tail_index);

    unsigned char *tail_ptr = self->tail->a + current_tail_pos + tail_index;
    
    if (!(*tail_ptr)) {
        log_debug("tail matches!\n");
        return token_index - 1;
    }

    log_debug("Searching tail: %s\n", tail_ptr);
    size_t num_tokens = tokens->n;
    for (int i = token_index; i < num_tokens; i++) {
        token_t token = tokens->a[i];

        char *ptr = str + token.offset;
        size_t token_length = token.len;

        if (!(*tail_ptr)) {
            log_debug("tail matches!\n");
            return i - 1;
        }

        if (token.type == WHITESPACE && *tail_ptr == ' ') continue;

        if (*tail_ptr == ' ') {
            tail_ptr++;
            log_debug("Got space, advancing pointer, tail_ptr=%s\n", tail_ptr);
        }

        log_debug("Tail string compare: %s with %.*s\n", tail_ptr, (int)token_length, ptr);

        if (strncmp((char *)tail_ptr, ptr, token_length) == 0) {
            tail_ptr += token_length;

            if (i == num_tokens - 1 && !(*tail_ptr)) {
                return i;
            }
        } else {
            return -1;
        }
    }
    return -1;

}


bool trie_search_tokens_from_index(trie_t *self, char *str, token_array *tokens, uint32_t start_node_id, phrase_array **phrases) {
    if (str == NULL || tokens == NULL || tokens->n == 0) return false;

    uint32_t node_id = start_node_id, last_node_id = start_node_id;
    trie_node_t node = trie_get_node(self, node_id), last_node = node;

    uint32_t data;

    int phrase_len = 0, phrase_start = 0, last_match_index = -1;

    trie_search_state_t state = SEARCH_STATE_BEGIN, last_state = SEARCH_STATE_BEGIN;

    token_t token;
    size_t token_length;

    log_debug("num_tokens: %zu\n", tokens->n);
    for (int i = 0; i < tokens->n; i++, last_state = state) {
        token = tokens->a[i];
        token_length = token.len;
        
        char *ptr = str + token.offset;
        log_debug("On %d, token=%.*s\n", i, (int)token_length, ptr);

        bool check_continuation = true;

        if (token.type != WHITESPACE) {
            for (int j = 0; j < token_length; j++, ptr++, last_node = node, last_node_id = node_id) {
                log_debug("Getting transition index for %d, (%d, %d)\n", node_id, node.base, node.check);
                size_t offset = j + 1;
                if (j > 0 || last_node.base >= 0) {
                    node_id = trie_get_transition_index(self, node, *ptr);
                    node = trie_get_node(self, node_id);
                    log_debug("Doing %c, got node_id=%d\n", *ptr, node_id);
                } else {
                    log_debug("Tail stored on space node, rolling back one character\n");
                    ptr--;
                    offset = j;
                    log_debug("ptr=%s\n", ptr);
                }

                if (node.check != last_node_id && last_node.base >= 0) {
                    log_debug("Fell off trie. last_node_id=%d and node.check=%d\n", last_node_id, node.check);
                    node_id = last_node_id = start_node_id;
                    node = last_node = trie_get_node(self, node_id);
                    break;
                } else if (node.base < 0) {
                    log_debug("Searching tail at index %d\n", i);

                    uint32_t data_index = -1*node.base;
                    trie_data_node_t data_node = self->data->a[data_index];
                    uint32_t current_tail_pos = data_node.tail;

                    unsigned char *current_tail = self->tail->a + current_tail_pos;

                    log_debug("token_length = %zu, j=%d\n", token_length, j);

                    size_t ptr_len = token_length - offset;
                    log_debug("next node tail: %s vs %.*s\n", current_tail, (int)ptr_len, ptr + 1);

                    if (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN) {
                        log_debug("phrase start at %d\n", i);
                        phrase_start = i;
                    }
                    if (strncmp((char *)current_tail, ptr + 1, ptr_len) == 0) {
                        log_debug("node tail matches first token\n");
                        int tail_search_result = trie_node_search_tail_tokens(self, node, str, tokens, ptr_len, i + 1);
                        log_debug("tail_search_result=%d\n", tail_search_result);
                        node_id = start_node_id;
                        node = trie_get_node(self, node_id);
                        check_continuation = false;

                        if (tail_search_result != -1) {
                            phrase_len = tail_search_result - phrase_start + 1;
                            last_match_index = i = tail_search_result;
                            last_state = SEARCH_STATE_MATCH;
                            data = data_node.data;
                        }
                        break;

                    } else {
                        node_id = start_node_id;
                        node = trie_get_node(self, node_id);
                        break;
                    }
                }
            }
        } else {
            check_continuation = false;
            if (state == SEARCH_STATE_BEGIN || state == SEARCH_STATE_NO_MATCH) {
                continue;
            }
        }


        if (node.check <= 0 || node_id == start_node_id) {
            log_debug("state = SEARCH_STATE_NO_MATCH\n");
            state = SEARCH_STATE_NO_MATCH;
            // check
            if (last_match_index != -1) {
                log_debug("last_match not NULL and state==SEARCH_STATE_NO_MATCH, data=%d\n", data);
                if (*phrases == NULL) {
                    *phrases = phrase_array_new_size(1);
                }
                phrase_array_push(*phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
                i = last_match_index;
                last_match_index = -1;
                phrase_start = phrase_len = 0;
                node_id = last_node_id = start_node_id;
                node = last_node = trie_get_node(self, start_node_id);
                continue;
            } else if (last_state == SEARCH_STATE_PARTIAL_MATCH) {
                // we're in the middle of a phrase that did not fully
                // match the trie. Imagine a trie that contains "a b c" and "b"
                // and our string is "a b d". When we get to "d", we've matched
                // the prefix "a b" and then will fall off the trie. So we'll actually 
                // need to go back to token "b" and start searching from the root

                // Note: i gets incremented with the next iteration of the for loop
                i = phrase_start;
                log_debug("last_state == SEARCH_STATE_PARTIAL_MATCH, i = %d\n", i);
                last_match_index = -1;
                phrase_start = phrase_len = 0;
                node_id = last_node_id = start_node_id;
                node = last_node = trie_get_node(self, start_node_id);
                continue;
            } else {
                phrase_start = phrase_len = 0;
                // this token was not a phrase
                log_debug("Plain token=%.*s\n", (int)token.len, str + token.offset);
            }
            node_id = last_node_id = start_node_id;
            node = last_node = trie_get_node(self, start_node_id);
        } else {

            state = SEARCH_STATE_PARTIAL_MATCH;
            if (!(node.base < 0) && (last_state == SEARCH_STATE_NO_MATCH || last_state == SEARCH_STATE_BEGIN)) {
                log_debug("phrase_start=%d, node.base = %d, last_state=%d\n", i, node.base, last_state);
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

                log_debug("phrase_start = %d\n", phrase_start);

                last_match_index = i;
                log_debug("last_match_index = %d\n", i);
            }

            if (i == tokens->n - 1) {
                if (last_match_index == -1) {
                    log_debug("At last token\n");
                    break;
                } else {
                    if (*phrases == NULL) {
                        *phrases = phrase_array_new_size(1);
                    }
                    phrase_array_push(*phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
                    i = last_match_index;
                    last_match_index = -1;
                    phrase_start = phrase_len = 0;
                    node_id = last_node_id = start_node_id;
                    node = last_node = trie_get_node(self, start_node_id);
                    state = SEARCH_STATE_NO_MATCH;
                    continue;
                }
            }

            if (check_continuation) {

                // Check continuation
                uint32_t continuation_id = trie_get_transition_index(self, node, ' ');
                log_debug("transition_id: %u\n", continuation_id);
                trie_node_t continuation = trie_get_node(self, continuation_id);

                if (token.type == IDEOGRAPHIC_CHAR && continuation.check != node_id) {
                    log_debug("Ideographic character\n");
                    last_node_id = node_id;
                    last_node = node;
                } else if (continuation.check != node_id && last_match_index != -1) {
                    log_debug("node->match no continuation\n");
                    if (*phrases == NULL) {
                        *phrases = phrase_array_new_size(1);
                    }
                    phrase_array_push(*phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
                    i = last_match_index;
                    last_match_index = -1;
                    phrase_start = phrase_len = 0;
                    node_id = last_node_id = start_node_id;
                    node = last_node = trie_get_node(self, start_node_id);
                    state = SEARCH_STATE_BEGIN;
                } else if (continuation.check != node_id) {
                    log_debug("No continuation for phrase with start=%d, yielding tokens\n", phrase_start);
                    state = SEARCH_STATE_NO_MATCH;
                    phrase_start = phrase_len = 0;
                    node_id = last_node_id = start_node_id;
                    node = last_node = trie_get_node(self, start_node_id);
                } else {
                    log_debug("Has continuation, node_id=%d\n", continuation_id);
                    last_node = node = continuation;
                    last_node_id = node_id = continuation_id;
                }            
            }
        }

    }

    if (last_match_index != -1) {
        if (*phrases == NULL) {
            *phrases = phrase_array_new_size(1);
        }
        log_debug("adding phrase, last_match_index=%d\n", last_match_index);
        phrase_array_push(*phrases, (phrase_t){phrase_start, last_match_index - phrase_start + 1, data});
   }

    return true;
}

inline bool trie_search_tokens_with_phrases(trie_t *self, char *str, token_array *tokens, phrase_array **phrases) {
    return trie_search_tokens_from_index(self, str, tokens, ROOT_NODE_ID, phrases);
}

inline phrase_array *trie_search_tokens(trie_t *self, char *str, token_array *tokens) {
    phrase_array *phrases = NULL;
    if (!trie_search_tokens_with_phrases(self, str, tokens, &phrases)) {
        return NULL;
    }
    return phrases;
}

phrase_t trie_search_suffixes_from_index(trie_t *self, char *word, size_t len, uint32_t start_node_id) {
    uint32_t last_node_id = start_node_id;
    trie_node_t last_node = trie_get_node(self, last_node_id);
    uint32_t node_id = last_node_id;
    trie_node_t node = last_node;

    uint32_t value = 0, phrase_start = 0, phrase_len = 0;

    ssize_t char_len;

    int32_t unich = 0;

    ssize_t index = len;
    const uint8_t *ptr = (const uint8_t *)word;
    const uint8_t *char_ptr;

    bool in_tail = false;
    unsigned char *current_tail = (unsigned char *)"";
    size_t tail_remaining = 0;

    uint32_t tail_value = 0;

    while(index > 0) {
        char_len = utf8proc_iterate_reversed(ptr, index, &unich);

        if (char_len <= 0) return NULL_PHRASE;
        if (!(utf8proc_codepoint_valid(unich))) return NULL_PHRASE;

        index -= char_len;
        char_ptr = ptr + index;

        if (in_tail && tail_remaining >= char_len && strncmp((char *)current_tail, (char *)char_ptr, char_len) == 0) {
            tail_remaining -= char_len;
            current_tail += char_len;
            phrase_start = (uint32_t)index;

            log_debug("tail matched at char %.*s (len=%zd)\n", (int)char_len, char_ptr, char_len);
            log_debug("tail_remaining = %zu\n", tail_remaining);

            if (tail_remaining == 0) {
                log_debug("tail match! tail_value=%u\n",tail_value);
                phrase_len = (uint32_t)(len - index);
                value = tail_value;
                index = 0;
                break;
            }
            continue;
        } else if (in_tail) {
            break;
        }

        for (int i=0; i < char_len; i++, char_ptr++, last_node = node, last_node_id = node_id) {
            log_debug("char=%c\n", (unsigned char)*char_ptr);

            node_id = trie_get_transition_index(self, node, *char_ptr);
            node = trie_get_node(self, node_id);
    
            if (node.check != last_node_id) {
                log_debug("node.check = %d and last_node_id = %d\n", node.check, last_node_id);
                index = 0;
                break;
            } else if (node.base < 0) {
                log_debug("Searching tail\n");

                uint32_t data_index = -1*node.base;
                trie_data_node_t data_node = self->data->a[data_index];
                uint32_t current_tail_pos = data_node.tail;

                tail_value = data_node.data;

                current_tail = self->tail->a + current_tail_pos;

                tail_remaining = strlen((char *)current_tail);
                log_debug("tail_remaining=%zu\n", tail_remaining);
                in_tail = true;

                size_t remaining_char_len = char_len - i - 1;
                log_debug("remaining_char_len = %zu\n", remaining_char_len);

                if (remaining_char_len > 0 && strncmp((char *)char_ptr + 1, (char *)current_tail, remaining_char_len) == 0) {
                    log_debug("tail string comparison successful\n");
                    tail_remaining -= remaining_char_len;
                    current_tail += remaining_char_len;
                } else if (remaining_char_len > 0) {
                    log_debug("tail comparison unsuccessful, %s vs %s\n", char_ptr, current_tail);
                    index = 0;
                    break;
                }

                if (tail_remaining == 0) {
                    phrase_start = (uint32_t)index;
                    phrase_len = (uint32_t)(len - index);
                    log_debug("phrase_start = %d, phrase_len=%d\n", phrase_start, phrase_len);
                    value = tail_value;
                    index = 0;
                }
                break;
            } else if (i == char_len - 1) {
                trie_node_t terminal_node = trie_get_transition(self, node, '\0');
                if (terminal_node.check == node_id) {
                    int32_t data_index = -1 * terminal_node.base;
                    trie_data_node_t data_node = self->data->a[data_index];
                    value = data_node.data;
                    phrase_start = (uint32_t)index;
                    phrase_len = (uint32_t)(len - index);
                }
            }

        }

    }

    return (phrase_t) {phrase_start, phrase_len, value};
}

inline phrase_t trie_search_suffixes_from_index_get_suffix_char(trie_t *self, char *word, size_t len, uint32_t start_node_id) {
    if (word == NULL || len == 0) return NULL_PHRASE;
    trie_node_t node = trie_get_node(self, start_node_id);
    unsigned char suffix_char = TRIE_SUFFIX_CHAR[0];
    uint32_t node_id = trie_get_transition_index(self, node, suffix_char);
    node = trie_get_node(self, node_id);

    if (node.check != start_node_id) {
        log_debug("node.check != start_node_id\n");
        return NULL_PHRASE;
    }

    return trie_search_suffixes_from_index(self, word, len, node_id);
}

inline phrase_t trie_search_suffixes(trie_t *self, char *word, size_t len) {
    if (word == NULL || len == 0) return NULL_PHRASE;
    return trie_search_suffixes_from_index_get_suffix_char(self, word, len, ROOT_NODE_ID);
}


phrase_t trie_search_prefixes_from_index(trie_t *self, char *word, size_t len, uint32_t start_node_id) {
    log_debug("Call to trie_search_prefixes_from_index\n");
    uint32_t node_id = start_node_id, last_node_id = node_id;
    trie_node_t node = trie_get_node(self, node_id), last_node = node;

    log_debug("last_node_id = %d\n", last_node_id);

    uint32_t value = 0, phrase_start = 0, phrase_len = 0;

    uint8_t *ptr = (uint8_t *)word;

    ssize_t char_len = 0;

    uint32_t idx = 0;

    size_t separator_char_len = 0;

    int32_t codepoint = 0;

    bool first_char = true;

    trie_data_node_t data_node;
    trie_node_t terminal_node;

    bool phrase_at_hyphen = false;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &codepoint);
        log_debug("char_len = %zu, char=%d\n", char_len, codepoint);
        if (char_len <= 0) break;
        if (!(utf8proc_codepoint_valid(codepoint))) break;

        bool is_hyphen = utf8_is_hyphen(codepoint);

        int cat = utf8proc_category(codepoint);
        bool is_space = utf8_is_separator(cat);

        uint8_t *char_ptr = ptr;
        size_t i = 0;

        bool skip_char = false;
        bool break_out = false;

        for (i = 0; i < char_len; i++) {
            node_id = trie_get_transition_index(self, last_node, *char_ptr);
            node = trie_get_node(self, node_id);
            log_debug("At idx=%u, i=%zu, char=%.*s\n", idx, i, (int)char_len, char_ptr);

            if (node.check != last_node_id) {
                log_debug("node.check = %d and last_node_id = %d\n", node.check, last_node_id);

                if (is_hyphen || (is_space && *ptr != ' ')) {
                    log_debug("Got hyphen or other separator, trying space instead\n");
                    node_id = trie_get_transition_index(self, last_node, ' ');
                    node = trie_get_node(self, node_id);
                }

                if (is_hyphen && node.check != last_node_id) {
                    log_debug("No space transition, phrase_len=%zu\n", phrase_len);
                    if (phrase_len > 0 && phrase_len == idx) {
                        log_debug("phrase_at_hyphen\n");
                        phrase_at_hyphen = true;
                    }

                    ptr += char_len;
                    idx += char_len;
                    separator_char_len = char_len;
                    node_id = last_node_id;
                    node = trie_get_node(self, node_id);
                    skip_char = true;
                    break;
                } else if (node.check != last_node_id) {
                    break_out = true;
                    log_debug("Breaking\n");
                    break;
                }
                break;
            }

            if (first_char) {
                phrase_start = idx;
                first_char = false;
            }

            if (node.base < 0) {
                log_debug("Searching tail\n");

                data_node = trie_get_data_node(self, node);
                uint32_t current_tail_pos = data_node.tail;

                unsigned char *current_tail = self->tail->a + current_tail_pos;

                log_debug("comparing tail: %s vs %s\n", current_tail, char_ptr + 1);
                size_t current_tail_len = strlen((char *)current_tail);

                size_t match_len = i + 1;
                size_t offset = i + 1;
                size_t tail_pos = 0;
                log_debug("offset=%zu\n", offset);

                if (char_len > 1) {
                    log_debug("char_len = %zu\n", char_len);
                    log_debug("Doing strncmp: (%zu) %s vs %s\n", char_len - offset, current_tail, char_ptr + 1);

                    if (strncmp((char *)ptr + offset, (char *)current_tail, char_len - offset) == 0) {
                        match_len += char_len - offset;
                        tail_pos = char_len - offset;
                        log_debug("in char match_len = %zu\n", match_len);
                    } else {
                        return NULL_PHRASE;
                    }
                }

                size_t tail_match_len = utf8_common_prefix_len((char *)ptr + char_len, (char *)current_tail + tail_pos, current_tail_len - tail_pos);
                match_len += tail_match_len;
                log_debug("match_len=%zu\n", match_len);
                
                if (tail_match_len == current_tail_len - tail_pos) {
                    if (phrase_at_hyphen) {
                        char_len = utf8proc_iterate(ptr + char_len, len, &codepoint);
                        if (char_len > 0 && utf8proc_codepoint_valid(codepoint)) {
                            int cat = utf8proc_category(codepoint);

                            if (codepoint != 0 && !utf8_is_hyphen(codepoint) && !utf8_is_separator(cat) && !utf8_is_punctuation(cat)) {
                                return (phrase_t){phrase_start, phrase_len, value};
                            }
                        }
                    }
                    if (first_char) phrase_start = idx;
                    phrase_len = (uint32_t)(idx + match_len) - phrase_start;

                    log_debug("tail match! phrase_len=%u, len=%zu\n", phrase_len, len);
                    value = data_node.data;
                    return (phrase_t){phrase_start, phrase_len, value};
                } else {
                    return NULL_PHRASE;
                }

            } else if (node.check == last_node_id) {
                terminal_node = trie_get_transition(self, node, '\0');
                log_debug("Trying link from %d to terminal node\n", last_node_id);

                if (terminal_node.check == node_id) {
                    log_debug("Transition to NUL byte matched\n");
                    if (terminal_node.base < 0) {
                        phrase_len = (uint32_t)(idx + char_len) - phrase_start;
                        data_node = trie_get_data_node(self, terminal_node);
                        value = data_node.data;
                    }
                    log_debug("Got match with len=%d\n", phrase_len);
                }
            }

            last_node = node;
            last_node_id = node_id;
            log_debug("last_node_id = %d\n", last_node_id);
            char_ptr++;
        }


        if (break_out) {
            break;
        } else if (skip_char) {
            continue;
        }

        log_debug("Incrementing index\n");

        idx += char_len;
        ptr += char_len;
    }

    log_debug("exited while loop\n");

    if (phrase_len == 0) return NULL_PHRASE;
 
    return (phrase_t) {phrase_start, phrase_len, value};
}

inline phrase_t trie_search_prefixes_from_index_get_prefix_char(trie_t *self, char *word, size_t len, uint32_t start_node_id) {
    trie_node_t node = trie_get_node(self, start_node_id);
    unsigned char prefix_char = TRIE_PREFIX_CHAR[0];
    uint32_t node_id = trie_get_transition_index(self, node, prefix_char);
    node = trie_get_node(self, node_id);

    if (node.check != start_node_id) {
        return NULL_PHRASE;
    }

    return trie_search_prefixes_from_index(self, word, len, node_id);
}

inline phrase_t trie_search_prefixes(trie_t *self, char *word, size_t len) {
    if (word == NULL || len == 0) return NULL_PHRASE;
    return trie_search_prefixes_from_index_get_prefix_char(self, word, len, ROOT_NODE_ID);
}

bool token_phrase_memberships(phrase_array *phrases, int64_array *phrase_memberships, size_t len) {
    if (phrases == NULL || phrase_memberships == NULL) {
        return false;
    }

    int64_t i = 0;
    for (int64_t j = 0; j < phrases->n; j++) {
        phrase_t phrase = phrases->a[j];

        for (; i < phrase.start; i++) {
            int64_array_push(phrase_memberships, NULL_PHRASE_MEMBERSHIP);
            log_debug("token i=%" PRId64 ", null phrase membership\n", i);
        }

        for (i = phrase.start; i < phrase.start + phrase.len; i++) {
            log_debug("token i=%" PRId64 ", phrase membership=%" PRId64 "\n", i, j);
            int64_array_push(phrase_memberships, j);
        }
    }

    for (; i < len; i++) {
        log_debug("token i=%" PRId64 ", null phrase membership\n", i);
        int64_array_push(phrase_memberships, NULL_PHRASE_MEMBERSHIP);
    }

    return true;
}

inline char *cstring_array_get_phrase(cstring_array *str, char_array *phrase_tokens, phrase_t phrase) {
    char_array_clear(phrase_tokens);

    size_t phrase_end = phrase.start + phrase.len;

    for (int k = phrase.start; k < phrase_end; k++) {
        char *w = cstring_array_get_string(str, k);
        char_array_append(phrase_tokens, w);
        if (k < phrase_end - 1) {
            char_array_append(phrase_tokens, " ");
        }
    }
    char_array_terminate(phrase_tokens);

    return char_array_get_string(phrase_tokens);
}

