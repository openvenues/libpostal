#ifndef GEONAMES_DICTIONARY_H
#define GEONAMES_DICTIONARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "bloom.h"
#include "config.h"
#include "geonames.h"
#include "sparkey/sparkey.h"
#include "sparkey/sparkey-internal.h"
#include "string_utils.h"
#include "trie.h"
#include "trie_search.h"

#define GEODB_TRIE_FILENAME "geodb.trie"
#define GEODB_TRIE_FILENAME_LEN strlen(GEODB_TRIE_FILENAME)
#define GEODB_BLOOM_FILTER_FILENAME "geodb.bloom"
#define GEODB_BLOOM_FILTER_FILENAME_LEN strlen(GEODB_BLOOM_FILTER_FILENAME)
#define GEODB_HASH_FILENAME "geodb.spi"
#define GEODB_HASH_FILENAME_LEN strlen(GEODB_HASH_FILENAME)
#define GEODB_LOG_FILENAME "geodb.spl"
#define GEODB_LOG_FILENAME_LEN strlen(GEODB_LOG_FILENAME)

// Can manipulate the bit-packed values separately, or access the whole value
typedef union geodb_value {
    uint32_t value;
    struct {
        uint32_t components:16;
        uint32_t count:16;
    };
} geodb_value_t;

typedef struct geodb {
    trie_t *trie;
    bloom_filter_t *bloom_filter;
    sparkey_hashreader *hash_reader;
    sparkey_logiter *log_iter;
    char_array *value_buf;
    geoname_t *geoname;
    gn_postal_code_t *postal_code;
} geodb_t;

typedef struct gn_geocoding_result {
    int start;
    int end;
    geonames_generic_t result;
} gn_geocoding_result_t;

geodb_t *get_geodb(void);
bool geodb_load(char *dir);

bool geodb_module_setup(char *dir);
void geodb_module_teardown(void);

void geodb_destroy(geodb_t *self);

geonames_generic_t *geodb_get_len(char *key, size_t len);
geonames_generic_t *geodb_get(char *key);

#ifdef __cplusplus
}
#endif

#endif