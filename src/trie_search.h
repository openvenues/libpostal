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

phrase_array *trie_search(trie_t *self, char *text);
phrase_array *trie_search_from_index(trie_t *self, char *text, uint32_t start_node_id);
phrase_array *trie_search_tokens(trie_t *self, char *str, token_array *tokens);
phrase_array *trie_search_tokens_from_index(trie_t *self, char *str, token_array *tokens, uint32_t start_node_id);
phrase_t trie_search_suffixes_from_index(trie_t *self, char *word, uint32_t start_node_id);
phrase_t trie_search_suffixes(trie_t *self, char *word);
phrase_t trie_search_prefixes_from_index(trie_t *self, char *word, uint32_t start_node_id);
phrase_t trie_search_prefixes(trie_t *self, char *word);

 

#endif