#ifndef TRIE_SEARCH_H
#define TRIE_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "trie.h"

#include "collections.h"
#include "klib/kvec.h"
#include "log/log.h"
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
phrase_array *trie_search_tokens(trie_t *self, tokenized_string_t *response);
phrase_t trie_search_suffixes(trie_t *self, char *word);

phrase_t trie_search_prefixes(trie_t *self, char *word);

#ifdef __cplusplus
}
#endif

#endif