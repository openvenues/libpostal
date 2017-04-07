#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "log/log.h"
#include "klib/khash.h"
#include "klib/ksort.h"
#include "vector.h"
#include "vector_math.h"

#define nop(x) (x)

// Init collections used in multiple places

// Maps

KHASH_MAP_INIT_INT(int_uint32, uint32_t)
KHASH_MAP_INIT_INT64(int64_uint32, uint32_t)
KHASH_MAP_INIT_INT64(int64_double, double)

#define kh_char_hash_func(key) (uint32_t)(key)
#define kh_char_hash_equal(a, b) ((a) == (b))

KHASH_INIT(char_uint32, char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)
KHASH_INIT(uchar_uint32, unsigned char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)

KHASH_MAP_INIT_STR(str_uint32, uint32_t)
KHASH_MAP_INIT_STR(str_double, double)
KHASH_MAP_INIT_INT(int_str, char *)
KHASH_MAP_INIT_STR(str_str, char *)

// Sets

KHASH_SET_INIT_INT(int_set)
KHASH_SET_INIT_INT64(int64_set)
KHASH_SET_INIT_STR(str_set)

// Vectors

VECTOR_INIT_NUMERIC(int32_array, int32_t, uint32_t, abs)
VECTOR_INIT_NUMERIC(uint32_array, uint32_t, uint32_t, nop)
VECTOR_INIT_NUMERIC(int64_array, int64_t, uint64_t, llabs)
VECTOR_INIT_NUMERIC(uint64_array, uint64_t, uint64_t, nop)
VECTOR_INIT_NUMERIC_FLOAT(float_array, float, fabsf)
VECTOR_INIT_NUMERIC_DOUBLE(double_array, double, fabs)

VECTOR_INIT(char_array, char)
VECTOR_INIT(uchar_array, unsigned char)
VECTOR_INIT(string_array, char *)

// Sorts

KSORT_INIT_STR

// Sort by value (must be defined after the vectors)

#define KHASH_SORT_BY_VALUE(name, key_type, val_type, val_array_name)                       \
    static key_type *name##_hash_sort_keys_by_value(khash_t(name) *h, bool reversed) {      \
        size_t n = kh_size(h);                                                              \
        key_type *keys = malloc(sizeof(key_type) * n);                                      \
        val_type *values = malloc(sizeof(val_type) * n);                                    \
                                                                                            \
        size_t i = 0;                                                                       \
        const key_type key;                                                                 \
        val_type value;                                                                     \
        kh_foreach(h, key, value, {                                                         \
            keys[i] = (key_type)key;                                                        \
            values[i] = value;                                                              \
            i++;                                                                            \
        })                                                                                  \
        size_t *sorted_indices = val_array_name##_argsort(values, n);                       \
        key_type *sorted_keys = malloc(sizeof(key_type) * n);                               \
                                                                                            \
        for (i = 0; i < n; i++) {                                                           \
            size_t idx = !reversed ? sorted_indices[i] : sorted_indices[n - i - 1];         \
            sorted_keys[i] = keys[idx];                                                     \
        }                                                                                   \
        free(keys);                                                                         \
        free(values);                                                                       \
        free(sorted_indices);                                                               \
        return sorted_keys;                                                                 \
    }

KHASH_SORT_BY_VALUE(str_uint32, char *, uint32_t, uint32_array)
KHASH_SORT_BY_VALUE(str_double, char *, double, double_array)

#define KHASH_GET(name, key_type, val_type)                                                                 \
    static bool name##_hash_get(khash_t(name) *h, key_type key, val_type *val) {                            \
        khiter_t k;                                                                                         \
        k = kh_get(name, h, (const key_type)key);                                                           \
        if (k != kh_end(h)) {                                                                               \
            *val = kh_value(h, k);                                                                          \
            return true;                                                                                    \
        }                                                                                                   \
        return false;                                                                                       \
    }

KHASH_GET(str_uint32, kh_cstr_t, uint32_t)
KHASH_GET(int_uint32, khint32_t, uint32_t)

#define KHASH_STR_INCR(name, val_type)                                                                      \
    static bool name##_hash_incr_by_exists(khash_t(name) *h, const char *key, val_type val, bool *exists) { \
        khiter_t k;                                                                                         \
        int ret = 0;                                                                                        \
        k = kh_get(name, h, key);                                                                           \
        if (k == kh_end(h)) {                                                                               \
            char *key_copy = strdup(key);                                                                   \
            if (key_copy == NULL) {                                                                         \
                return false;                                                                               \
            }                                                                                               \
            k = kh_put(name, h, key_copy, &ret);                                                            \
            if (ret < 0) {                                                                                  \
                free(key_copy);                                                                             \
                return false;                                                                               \
            }                                                                                               \
            kh_value(h, k) = (val_type)0;                                                                   \
            *exists = false;                                                                                \
        } else {                                                                                            \
            *exists = true;                                                                                 \
        }                                                                                                   \
        kh_value(h, k) += val;                                                                              \
        return true;                                                                                        \
    }                                                                                                       \
    static bool name##_hash_incr_exists(khash_t(name) *h, const char *key, bool *exists) {                  \
        return name##_hash_incr_by_exists(h, key, (val_type)1, exists);                                     \
    }                                                                                                       \
    static bool name##_hash_incr_by(khash_t(name) *h, const char *key, val_type val) {                      \
        bool exists = false;                                                                                \
        return name##_hash_incr_by_exists(h, key, val, &exists);                                            \
    }                                                                                                       \
    static bool name##_hash_incr(khash_t(name) *h, const char *key) {                                       \
        bool exists = false;                                                                                \
        return name##_hash_incr_by_exists(h, key, (val_type)1, &exists);                                    \
    }

KHASH_STR_INCR(str_uint32, uint32_t)

#define KHASH_STR_TO_ID(name, val_type)                                                                     \
    static bool name##_hash_to_id_exists(khash_t(name) *h, const char *key, val_type *val, bool *exists) {  \
        khiter_t k;                                                                                         \
        int ret = 0;                                                                                        \
        k = kh_get(name, h, key);                                                                           \
        if (k == kh_end(h)) {                                                                               \
            char *key_copy = strdup(key);                                                                   \
            val_type new_id = (val_type)kh_size(h);                                                         \
            if (key_copy == NULL) {                                                                         \
                return false;                                                                               \
            }                                                                                               \
            k = kh_put(name, h, key_copy, &ret);                                                            \
            if (ret < 0) {                                                                                  \
                free(key_copy);                                                                             \
                return false;                                                                               \
            }                                                                                               \
            kh_value(h, k) = new_id;                                                                        \
            *val = new_id;                                                                                  \
            *exists = false;                                                                                \
            return true;                                                                                    \
        } else {                                                                                            \
            *exists = true;                                                                                 \
        }                                                                                                   \
        *val = kh_value(h, k);                                                                              \
        return true;                                                                                        \
    }                                                                                                       \
    static bool name##_hash_to_id(khash_t(name) *h, const char *key, val_type *val) {                       \
        bool exists = false;                                                                                \
        return name##_hash_to_id_exists(h, key, val, &exists);                                              \
    }

KHASH_STR_TO_ID(str_uint32, uint32_t)

#define KHASH_INCR(name, key_type, val_type)                                                                \
    static bool name##_hash_incr_by_exists(khash_t(name) *h, key_type key, val_type val, bool *exists) {    \
        khiter_t k;                                                                                         \
        int ret = 0;                                                                                        \
        k = kh_get(name, h, key);                                                                           \
        if (k == kh_end(h)) {                                                                               \
            k = kh_put(name, h, key, &ret);                                                                 \
            if (ret < 0) {                                                                                  \
                return false;                                                                               \
            }                                                                                               \
            kh_value(h, k) = (val_type)0;                                                                   \
            *exists = false;                                                                                \
        } else {                                                                                            \
            *exists = true;                                                                                 \
        }                                                                                                   \
        kh_value(h, k) += val;                                                                              \
        return true;                                                                                        \
    }                                                                                                       \
    static bool name##_hash_incr_exists(khash_t(name) *h, key_type key, bool *exists) {                     \
        return name##_hash_incr_by_exists(h, key, (val_type)1, exists);                                     \
    }                                                                                                       \
    static bool name##_hash_incr_by(khash_t(name) *h, key_type key, val_type val) {                         \
        bool exists = false;                                                                                \
        return name##_hash_incr_by_exists(h, key, val, &exists);                                            \
    }                                                                                                       \
    static bool name##_hash_incr(khash_t(name) *h, key_type key) {                                          \
        bool exists = false;                                                                                \
        return name##_hash_incr_by_exists(h, key, (val_type)1, &exists);                                    \
    }

KHASH_INCR(int_uint32, khint32_t, uint32_t)
KHASH_INCR(int64_uint32, khint64_t, uint32_t)
KHASH_INCR(int64_double, khint64_t, double)

#endif
