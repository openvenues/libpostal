#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "vector.h"

#define ks_lt_index(a, b) ((a).value < (b).value)

#define VECTOR_INIT_NUMERIC(name, type, unsigned_type, type_abs)                                    \
    __VECTOR_BASE(name, type)                                                                       \
    __VECTOR_DESTROY(name, type)                                                                    \
                                                                                                    \
    static inline void name##_zero(type *array, size_t n) {                                         \
        memset(array, 0, n * sizeof(type));                                                         \
    }                                                                                               \
                                                                                                    \
    static inline void name##_set(type *array, size_t n, type value) {                              \
        for (int i = 0; i < n; i++) {                                                               \
            array[i] = value;                                                                       \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline name *name##_new_value(size_t n, type value) {                                    \
        name *vector = name##_new_size(n);                                                          \
        if (vector == NULL) return NULL;                                                            \
        name##_set(vector->a, n, (type)value);                                                      \
        vector->n = n;                                                                              \
        return vector;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline name *name##_new_ones(size_t n) {                                                 \
        return name##_new_value(n, (type)1);                                                        \
    }                                                                                               \
                                                                                                    \
    static inline name *name##_new_zeros(size_t n) {                                                \
        name *vector = name##_new_size(n);                                                          \
        name##_zero(vector->a, n);                                                                  \
        vector->n = n;                                                                              \
        return vector;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline type name##_max(type *array, size_t n) {                                          \
        if (n < 1) return (type) 0;                                                                 \
        type val = array[0];                                                                        \
        type max_val = val;                                                                         \
        for (int i = 1; i < n; i++) {                                                               \
            val = array[i];                                                                         \
            if (val > max_val) max_val = val;                                                       \
        }                                                                                           \
        return max_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    static inline type name##_min(type *array, size_t n) {                                          \
        if (n < 1) return (type) 0;                                                                 \
        type val = array[0];                                                                        \
        type min_val = val;                                                                         \
        for (int i = 1; i < n; i++) {                                                               \
            val = array[i];                                                                         \
            if (val < min_val) min_val = val;                                                       \
        }                                                                                           \
        return min_val;                                                                             \
    }                                                                                               \
                                                                                                    \
    static inline int64_t name##_argmax(type *array, size_t n) {                                    \
        if (n < 1) return -1;                                                                       \
        type val = array[0];                                                                        \
        type max_val = val;                                                                         \
        int64_t argmax = 0;                                                                         \
        for (int i = 0; i < n; i++) {                                                               \
            val = array[i];                                                                         \
            if (val > max_val) {                                                                    \
                max_val = val;                                                                      \
                argmax = i;                                                                         \
            }                                                                                       \
        }                                                                                           \
        return argmax;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline int64_t name##_argmin(type *array, size_t n) {                                    \
        if (n < 1) return (type) -1;                                                                \
        type val = array[0];                                                                        \
        type min_val = val;                                                                         \
        int64_t argmin = 0;                                                                         \
        for (int i = 1; i < n; i++) {                                                               \
            val = array[i];                                                                         \
            if (val < min_val) {                                                                    \
                min_val = val;                                                                      \
                argmin = i;                                                                         \
            }                                                                                       \
        }                                                                                           \
        return argmin;                                                                              \
    }                                                                                               \
                                                                                                    \
    typedef struct type##_index {                                                                   \
        size_t index;                                                                               \
        type value;                                                                                 \
    } type##_index_t;                                                                               \
                                                                                                    \
    KSORT_INIT_GENERIC(type)                                                                        \
    KSORT_INIT(type##_indices, type##_index_t, ks_lt_index)                                         \
                                                                                                    \
    static inline void name##_sort(type *array, size_t n) {                                         \
        ks_introsort(type, n, array);                                                               \
    }                                                                                               \
                                                                                                    \
    static inline size_t *name##_argsort(type *array, size_t n) {                                   \
        type##_index_t *type_indices = malloc(sizeof(type##_index_t) * n);                          \
        size_t i;                                                                                   \
        for (i = 0; i < n; i++) {                                                                   \
            type_indices[i] = (type##_index_t){i, array[i]};                                        \
        }                                                                                           \
        ks_introsort(type##_indices, n, type_indices);                                              \
        size_t *indices = malloc(sizeof(size_t) * n);                                               \
        for (i = 0; i < n; i++) {                                                                   \
            indices[i] = type_indices[i].index;                                                     \
        }                                                                                           \
        free(type_indices);                                                                         \
        return indices;                                                                             \
    }                                                                                               \
                                                                                                    \
    static inline void name##_add(type *array, type c, size_t n) {                                  \
        for (int i = 0; i < n; i++) {                                                               \
            array[i] += c;                                                                          \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_sub(type *array, type c, size_t n) {                                  \
        for (int i = 0; i < n; i++) {                                                               \
            array[i] -= c;                                                                          \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_mul(type *array, type c, size_t n) {                                  \
        for (int i = 0; i < n; i++) {                                                               \
            array[i] *= c;                                                                          \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_div(type *array, type c, size_t n) {                                  \
        for (int i = 0; i < n; i++) {                                                               \
            array[i] /= c;                                                                          \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline type name##_sum(type *array, size_t n) {                                          \
        type result = 0;                                                                            \
        for (int i = 0; i < n; i++) {                                                               \
            result += array[i];                                                                     \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline unsigned_type name##_l1_norm(type *array, size_t n) {                             \
        unsigned_type result = 0;                                                                   \
        for (int i = 0; i < n; i++) {                                                               \
            result += type_abs(array[i]);                                                           \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline unsigned_type name##_l2_norm(type *array, size_t n) {                             \
        unsigned_type result = 0;                                                                   \
        for (int i = 0; i < n; i++) {                                                               \
            result += array[i] * array[i];                                                          \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline type name##_product(type *array, size_t n) {                                      \
        type result = 0;                                                                            \
        for (int i = 0; i < n; i++) {                                                               \
            result *= array[i];                                                                     \
        }                                                                                           \
        return result;                                                                              \
    }                                                                                               \
                                                                                                    \
    static inline void name##_add_array(type *a1, type *a2, size_t n) {                             \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] += a2[i];                                                                         \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_add_array_times_scalar(type *a1, type *a2, double v, size_t n) {      \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] += a2[i] * v;                                                                     \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_sub_array(type *a1, type *a2, size_t n) {                             \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] -= a2[i];                                                                         \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
                                                                                                    \
    static inline void name##_sub_array_times_scalar(type *a1, type *a2, double v, size_t n) {      \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] -= a2[i] * v;                                                                     \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_mul_array(type *a1, type *a2, size_t n) {                             \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] *= a2[i];                                                                         \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_mul_array_times_scalar(type *a1, type *a2, double v, size_t n) {      \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] *= a2[i] * v;                                                                     \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_div_array(type *a1, type *a2, size_t n) {                             \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] /= a2[i];                                                                         \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline void name##_div_array_times_scalar(type *a1, type *a2, double v, size_t n) {      \
        for (int i = 0; i < n; i++) {                                                               \
            a1[i] /= a2[i] * v;                                                                     \
        }                                                                                           \
    }                                                                                               \
                                                                                                    \
    static inline type name##_dot(type *a1, type *a2, size_t n) {                                   \
        type result = 0;                                                                            \
        for (int i = 0; i < n; i++) {                                                               \
            result += a1[i] * a2[i];                                                                \
        }                                                                                           \
        return result;                                                                              \
    }



#define VECTOR_INIT_NUMERIC_FLOAT(name, type, type_abs)                        \
    VECTOR_INIT_NUMERIC(name, type, type, type_abs)                            \
                                                                               \
    static inline void name##_log(type *array, size_t n) {                     \
        for (int i = 0; i < n; i++) {                                          \
            array[i] = log(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_exp(type *array, size_t n) {                     \
        for (int i = 0; i < n; i++) {                                          \
            array[i] = exp(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum(type *array, size_t n) {                 \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += log(array[i]);                                           \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum_exp(type *array, size_t n) {             \
        type max = name##_max(array, n);                                       \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += exp(array[i] - max);                                     \
        }                                                                      \
        return max + log(result);                                              \
    }

#endif