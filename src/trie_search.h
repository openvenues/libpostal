#ifndef TRIE_SEARCH_H
#define TRIE_SEARCH_H

 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "trie.h"

#include "collections.h"
#include "klib/kvec.h"
#include "log/log.h"
#include "string_utils.h"
#include "tokens.h"
#include "vector.h"
#include "utf8proc/utf8proc.h"

typedef struct phrase {
    uint32_t start;
    uint32_t len;
    uint32_t data;
} phrase_t;

VECTOR_INIT(phrase_array, phrase_t)

#define NULL_PHRASE (phrase_t){0, 0, 0}
#define NULL_PHRASE_MEMBERSHIP -1

phrase_array *trie_search(trie_t *self, char *text);
bool trie_search_from_index(trie_t *self, char *text, uint32_t start_node_id, phrase_array **phrases);
bool trie_search_with_phrases(trie_t *self, char *text, phrase_array **phrases);
phrase_array *trie_search_tokens(trie_t *self, char *str, token_array *tokens);
bool trie_search_tokens_from_index(trie_t *self, char *str, token_array *tokens, uint32_t start_node_id, phrase_array **phrases);
bool trie_search_tokens_with_phrases(trie_t *self, char *text, token_array *tokens, phrase_array **phrases);
phrase_t trie_search_suffixes_from_index(trie_t *self, char *word, size_t len, uint32_t start_node_id);
phrase_t trie_search_suffixes_from_index_get_suffix_char(trie_t *self, char *word, size_t len, uint32_t start_node_id);
phrase_t trie_search_suffixes(trie_t *self, char *word, size_t len);
phrase_t trie_search_prefixes_from_index(trie_t *self, char *word, size_t len, uint32_t start_node_id);
phrase_t trie_search_prefixes_from_index_get_prefix_char(trie_t *self, char *word, size_t len, uint32_t start_node_id);
phrase_t trie_search_prefixes(trie_t *self, char *word, size_t len);

bool token_phrase_memberships(phrase_array *phrases, int64_array *phrase_memberships, size_t len);

char *cstring_array_get_phrase(cstring_array *str, char_array *phrase_tokens, phrase_t phrase);

#endif
