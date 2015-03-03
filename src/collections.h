#ifndef COLLECTIONS_H
#define COLLECTIONS_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "klib/khash.h"
#include "vector.h"

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

// Vectors

VECTOR_INIT(int32_array, int32_t)
VECTOR_INIT(uint32_array, uint32_t)
VECTOR_INIT(char_array, char)
VECTOR_INIT(uchar_array, unsigned char)


#ifdef __cplusplus
}
#endif

#endif