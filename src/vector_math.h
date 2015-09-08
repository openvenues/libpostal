#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "vector.h"

#define VECTOR_INIT_NUMERIC(name, type)                                        \
    __VECTOR_BASE(name, type)                                                  \
    __VECTOR_DESTROY(name, type)                                               \
                                                                               \
    static inline void type##_array_set(type *array, size_t n, type value) {   \
        for (int i = 0; i < n; i++) {                                          \
            array[i] = value;                                                  \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline name *name##_new_value(size_t n, type value) {               \
        name *vector = name##_new_size(n);                                     \
        if (vector == NULL) return NULL;                                       \
        type##_array_set(vector->a, n, (type)value);                           \
        vector->n = n;                                                         \
        return vector;                                                         \
    }                                                                          \
                                                                               \
    static inline name *name##_new_ones(size_t n) {                            \
        return name##_new_value(n, (type)1);                                   \
    }                                                                          \
                                                                               \
    static inline name *name##_new_zeros(size_t n) {                           \
        name *vector = name##_new_size(n);                                     \
        memset(vector->a, 0, n * sizeof(type));                                \
        vector->n = n;                                                         \
        return name##_new_value(n, (type)0);                                   \
    }                                                                          \
                                                                               \
    static inline type type##_array_max(type *array, size_t n) {               \
        if (n < 1) return (type) 0;                                            \
        type val = array[0];                                                   \
        type max_val = val;                                                    \
        for (int i = 1; i < n; i++) {                                          \
            val = array[i];                                                    \
            if (val > max_val) max_val = val;                                  \
        }                                                                      \
        return max_val;                                                        \
    }                                                                          \
                                                                               \
    static inline type type##_array_min(type *array, size_t n) {               \
        if (n < 1) return (type) 0;                                            \
        type val = array[0];                                                   \
        type min_val = val;                                                    \
        for (int i = 1; i < n; i++) {                                          \
            val = array[i];                                                    \
            if (val < min_val) min_val = val;                                  \
        }                                                                      \
        return min_val;                                                        \
    }                                                                          \
                                                                               \
    static inline int64_t type##_array_argmax(type *array, size_t n) {         \
        if (n < 1) return -1;                                                  \
        type val = array[0];                                                   \
        type max_val = val;                                                    \
        int64_t argmax = 0;                                                    \
        for (int i = 0; i < n; i++) {                                          \
            val = array[i];                                                    \
            if (val > max_val) {                                               \
                max_val = val;                                                 \
                argmax = i;                                                    \
            }                                                                  \
        }                                                                      \
        return argmax;                                                         \
    }                                                                          \
                                                                               \
    static inline int64_t type##_array_argmin(type *array, size_t n) {         \
        if (n < 1) return (type) -1;                                           \
        type val = array[0];                                                   \
        type min_val = val;                                                    \
        int64_t argmin = 0;                                                    \
        for (int i = 1; i < n; i++) {                                          \
            val = array[i];                                                    \
            if (val < min_val) {                                               \
                min_val = val;                                                 \
                argmin = i;                                                    \
            }                                                                  \
        }                                                                      \
        return argmin;                                                         \
    }                                                                          \
                                                                               \
    static inline void type##_array_add(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] += c;                                                     \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_sub(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] -= c;                                                     \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_mul(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] *= c;                                                     \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_div(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] /= c;                                                     \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type type##_array_sum(type *array, size_t n) {               \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += array[i];                                                \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type type##_array_l1_norm(type *array, size_t n) {           \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += abs(array[i]);                                           \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type type##_array_l2_norm(type *array, size_t n) {           \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += array[i] * array[i];                                     \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type type##_array_product(type *array, size_t n) {           \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result *= array[i];                                                \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline void type##_array_add_array(type *a1, type *a2, size_t n) {  \
        for (int i = 0; i < n; i++) {                                          \
            a1[i] += a2[i];                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_sub_array(type *a1, type *a2, size_t n) {  \
        for (int i = 0; i < n; i++) {                                          \
            a1[i] -= a2[i];                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_mul_array(type *a1, type *a2, size_t n) {  \
        for (int i = 0; i < n; i++) {                                          \
            a1[i] *= a2[i];                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_div_array(type *a1, type *a2, size_t n) {  \
        for (int i = 0; i < n; i++) {                                          \
            a1[i] /= a2[i];                                                    \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type type##_array_dot(type *a1, type *a2, size_t n) {        \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += a1[i] * a2[i];                                           \
        }                                                                      \
        return result;                                                         \
    }



#define VECTOR_INIT_NUMERIC_FLOAT(name, type)                                  \
    VECTOR_INIT_NUMERIC(name, type)                                            \
                                                                               \
    static inline void type##_array_log(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] = log(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void type##_array_exp(type *array, type c, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            array[i] = exp(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type type##_array_log_sum(type *array, size_t n) {           \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += log(array[i]);                                           \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type type##_array_log_sum_exp(type *array, size_t n) {       \
        type max = type##_array_max(array, n);                                 \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += exp(array[i] - max);                                     \
        }                                                                      \
        return max + log(result);                                              \
    }


#endif