#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

#include "klib/khash.h"
#include "vector.h"
#include "vector_math.h"

// Init collections used in multiple places

// Maps

KHASH_MAP_INIT_INT(int_int, uint32_t)

#define kh_char_hash_func(key) (uint32_t)(key)
#define kh_char_hash_equal(a, b) ((a) == (b))

KHASH_INIT(char_int, char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)
KHASH_INIT(uchar_int, unsigned char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)

KHASH_MAP_INIT_STR(str_int, uint32_t)
KHASH_MAP_INIT_INT(int_str, char *)
KHASH_MAP_INIT_STR(str_str, char *)

// Sets

KHASH_SET_INIT_INT(int_set)
KHASH_SET_INIT_STR(str_set)

// Vectors

VECTOR_INIT_NUMERIC(int32_array, int32_t)
VECTOR_INIT_NUMERIC(uint32_array, uint32_t)
VECTOR_INIT_NUMERIC(int64_array, int64_t)
VECTOR_INIT_NUMERIC(uint64_array, uint64_t)
VECTOR_INIT_NUMERIC(float_array, float)
VECTOR_INIT_NUMERIC(double_array, double)

VECTOR_INIT(char_array, char)
VECTOR_INIT(uchar_array, unsigned char)

#ifdef __cplusplus
}
#endif

#endif
