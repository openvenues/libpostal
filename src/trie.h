/******************************************************************************
* Double-array trie implementation for compactly storing large dictionaries.
* This trie differs from most implmentations in that it stores all of the tails
* (compressed) in a single contiguous character array, separated by NUL-bytes,
* so given an index into that array, we can treat the array as a C string
* starting at that index. It also makes serialization dead simple. We
* implement a novel scheme for storing reversed strings (suffixes, etc.) A suffix
* is defined as the reversed UTF-8 suffix string prefixed by TRIE_SUFFIX_CHAR.
* Similarly, a prefix is defined as being prefixed by TRIE_PREFIX_CHAR. 
* trie_search defines several methods for searching strings, tokenized strings,
* prefixes and suffixes. Note that the single characters TRIE_SUFFIX_CHAR 
* and TRIE_PREFIX_CHAR are not allowed as keys (both are defined as control 
* characters, so are unlikely to affect natural language applications).
* This trie implementation also has several *_from_index methods which allow 
* for effective namespacing e.g. adding the keys "en|blvd" and "fr|blvd"
* and searching by language. For more information on double-array tries
* generally, see: http://linux.thai.net/~thep/datrie/datrie.html
******************************************************************************/

#ifndef TRIE_H
#define TRIE_H

 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "collections.h"
#include "file_utils.h"
#include "klib/kvec.h"
#include "log/log.h"
#include "string_utils.h"

#define TRIE_SIGNATURE 0xABABABAB
#define NULL_NODE_ID 0
#define FREE_LIST_ID 1
#define ROOT_NODE_ID 2
#define TRIE_POOL_BEGIN 3
#define DEFAULT_NODE_ARRAY_SIZE 32

#define TRIE_INDEX_ERROR  0
#define TRIE_MAX_INDEX 0x7fffffff

#define TRIE_PREFIX_CHAR "\x02"
#define TRIE_SUFFIX_CHAR "\x03"

// Using 256 characters can fit all UTF-8 encoded strings
#define NUM_CHARS 256

typedef struct trie_node {
    int32_t base;
    int32_t check;
} trie_node_t;

#define NULL_NODE (trie_node_t){0, 0}

typedef struct trie_data_node {
    uint32_t tail;
    uint32_t data;
} trie_data_node_t;

#define NULL_DATA_NODE (trie_data_node_t){0, 0};

VECTOR_INIT(trie_node_array, trie_node_t)
VECTOR_INIT(trie_data_array, trie_data_node_t)

typedef struct trie {
    trie_node_t null_node;
    trie_node_array *nodes;
    trie_data_array *data;
    uchar_array *tail;
    char *alphabet;
    uint8_t alpha_map[NUM_CHARS];
    uint32_t alphabet_size;
    uint32_t num_keys;
} trie_t;

trie_t *trie_new_alphabet(uint8_t *alphabet, uint32_t alphabet_size);
trie_t *trie_new(void);

uint32_t trie_get_char_index(trie_t *self, unsigned char c);
uint32_t trie_get_transition_index(trie_t *self, trie_node_t node, unsigned char c);
trie_node_t trie_get_transition(trie_t *self, trie_node_t node, unsigned char c);
bool trie_node_is_free(trie_node_t node);


trie_node_t trie_get_node(trie_t *self, uint32_t index);
void trie_set_base(trie_t *self, uint32_t index, int32_t base);
void trie_set_check(trie_t *self, uint32_t index, int32_t check);
trie_node_t trie_get_root(trie_t *self);
trie_node_t trie_get_free_list(trie_t *self);

trie_data_node_t trie_get_data_node(trie_t *self, trie_node_t node);
bool trie_set_data_node(trie_t *self, uint32_t index, trie_data_node_t data_node);

bool trie_get_data_at_index(trie_t *self, uint32_t index,  uint32_t *data);
bool trie_get_data(trie_t *self, char *key, uint32_t *data);
bool trie_set_data_at_index(trie_t *self, uint32_t index, uint32_t data);
bool trie_set_data(trie_t *self, char *key, uint32_t data);

bool trie_tail_match(trie_t *self, char *str, uint32_t tail_index);

uint32_t trie_add_transition(trie_t *self, uint32_t node_id, unsigned char c);

void trie_make_room_for(trie_t *self, uint32_t next_id);

void trie_add_tail(trie_t *self, unsigned char *tail);
void trie_set_tail(trie_t *self, unsigned char *tail, uint32_t tail_pos);
int32_t trie_separate_tail(trie_t *self, uint32_t from_index, unsigned char *tail, uint32_t data);
void trie_tail_merge(trie_t *self, uint32_t old_node_id, unsigned char *suffix, uint32_t data);

bool trie_add_at_index(trie_t *self, uint32_t node_id, char *key, size_t len, uint32_t data);
bool trie_add(trie_t *self, char *key, uint32_t data);
bool trie_add_len(trie_t *self, char *key, size_t len, uint32_t data);
bool trie_add_suffix(trie_t *self, char *key, uint32_t data);
bool trie_add_suffix_at_index(trie_t *self, char *key, uint32_t start_node_id, uint32_t data);
bool trie_add_prefix(trie_t *self, char *key, uint32_t data);
bool trie_add_prefix_at_index(trie_t *self, char *key, uint32_t start_node_id, uint32_t data);

uint32_t trie_get_from_index(trie_t *self, char *word, size_t len, uint32_t i);
uint32_t trie_get_len(trie_t *self, char *word, size_t len);
uint32_t trie_get(trie_t *self, char *word);

uint32_t trie_num_keys(trie_t *self);

typedef struct trie_prefix_result {
    uint32_t node_id;
    size_t tail_pos;
} trie_prefix_result_t;

#define ROOT_PREFIX_RESULT (trie_prefix_result_t) {ROOT_NODE_ID, 0}
#define NULL_PREFIX_RESULT (trie_prefix_result_t) {NULL_NODE_ID, 0}

trie_prefix_result_t trie_get_prefix(trie_t *self, char *key);
trie_prefix_result_t trie_get_prefix_len(trie_t *self, char *key, size_t len);
trie_prefix_result_t trie_get_prefix_from_index(trie_t *self, char *key, size_t len, uint32_t start_index, size_t tail_pos);

void trie_print(trie_t *self);

bool trie_write(trie_t *self, FILE *file);
bool trie_save(trie_t *self, char *path);

trie_t *trie_read(FILE *file);
trie_t *trie_load(char *path);

void trie_destroy(trie_t *self);


 
#endif
