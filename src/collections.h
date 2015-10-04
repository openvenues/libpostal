#ifndef COLLECTIONS_H
#define COLLECTIONS_H

 

#include <stdlib.h>
#include <stdint.h>

#include "klib/khash.h"
#include "klib/ksort.h"
#include "vector.h"
#include "vector_math.h"

// Init collections used in multiple places

// Maps

KHASH_MAP_INIT_INT(int_uint32, uint32_t)

#define kh_char_hash_func(key) (uint32_t)(key)
#define kh_char_hash_equal(a, b) ((a) == (b))

KHASH_INIT(char_uint32, char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)
KHASH_INIT(uchar_uint32, unsigned char, uint32_t, 1, kh_char_hash_func, kh_char_hash_equal)

KHASH_MAP_INIT_STR(str_uint32, uint32_t)
KHASH_MAP_INIT_INT(int_str, char *)
KHASH_MAP_INIT_STR(str_str, char *)

// Sets

KHASH_SET_INIT_INT(int_set)
KHASH_SET_INIT_STR(str_set)

// Vectors

VECTOR_INIT_NUMERIC(int32_array, int32_t, uint32_t, abs)
VECTOR_INIT_NUMERIC(uint32_array, uint32_t, uint32_t, abs)
VECTOR_INIT_NUMERIC(int64_array, int64_t, uint64_t, llabs)
VECTOR_INIT_NUMERIC(uint64_array, uint64_t, uint64_t, llabs)
VECTOR_INIT_NUMERIC_FLOAT(float_array, float, fabsf)
VECTOR_INIT_NUMERIC_FLOAT(double_array, double, fabs)

VECTOR_INIT(char_array, char)
VECTOR_INIT(uchar_array, unsigned char)
VECTOR_INIT(string_array, char *)

KSORT_INIT_STR

#endif
