#ifndef TRIE_UTILS_H
#define TRIE_UTILS_H

#include "collections.h"
#include "string_utils.h"
#include "trie.h"

trie_t *trie_new_from_hash(khash_t(str_uint32) *hash);
trie_t *trie_new_from_cstring_array_sorted(cstring_array *strings);
trie_t *trie_new_from_cstring_array(cstring_array *strings);

#endif