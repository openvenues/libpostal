#include "trie_utils.h"

/*
Build a trie from the sorted keys of a hashtable. Adding
keys in sorted order to a double-array trie is faster than
adding them in random order.
*/
trie_t *trie_new_from_hash(khash_t(str_uint32) *hash) {
    trie_t *trie = trie_new();
    const char *key;
    uint32_t value;

    size_t hash_size = kh_size(hash);
    log_info("hash_size=%zu\n", hash_size);
    string_array *hash_keys = string_array_new_size(hash_size);
    kh_foreach(hash, key, value, {
        if (strlen(key) == 0) continue;
        string_array_push(hash_keys, (char *)key);
    })

    ks_introsort(str, hash_keys->n, (const char **)hash_keys->a);

    khiter_t k;

    for (int i = 0; i < hash_keys->n; i++) {
        char *str = hash_keys->a[i];
        k = kh_get(str_uint32, hash, str);
        if (k == kh_end(hash)) {
            log_error("Key not found\n");
            string_array_destroy(hash_keys);
            trie_destroy(trie);
            return NULL;
        }

        value = kh_value(hash, k);

        if (!trie_add(trie, str, value)) {
            log_error("Error adding to trie\n");
            string_array_destroy(hash_keys);
            trie_destroy(trie);
            return NULL;
        }
        if (i % 100000 == 0 && i > 0) {
            log_info("added %d keys to trie\n", i);
        }
    }

    string_array_destroy(hash_keys);

    return trie;
}

trie_t *trie_new_from_cstring_array_sorted(cstring_array *strings) {
    char *key;
    uint32_t i;

    int ret = 0;
    uint32_t next_id = 0;

    size_t n = cstring_array_num_strings(strings);

    khash_t(str_uint32) *hash = kh_init(str_uint32);
    kh_resize(str_uint32, hash, n);

    cstring_array_foreach(strings, i, key, {
        if (strlen(key) == 0) continue;

        khiter_t k = kh_put(str_uint32, hash, key, &ret);

        if (ret < 0) {
            kh_destroy(str_uint32, hash);
            return NULL;
        }

        kh_value(hash, k) = next_id++;
    })

    trie_t *trie = trie_new_from_hash(hash);
    kh_destroy(str_uint32, hash);

    return trie;
}

trie_t *trie_new_from_cstring_array(cstring_array *strings) {
    char *key;
    uint32_t i;

    uint32_t next_id;

    trie_t *trie = trie_new();

    cstring_array_foreach(strings, i, key, {
        if (strlen(key) == 0) continue;
        if (!trie_add(trie, key, next_id++)) {
            trie_destroy(trie);
            return NULL;
        }
    })

    return trie;
}
