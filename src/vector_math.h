#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <stdlib.h>
#include <math.h>
#include "vector.h"

#define VECTOR_INIT_NUMERIC(name, type)                                        \
    __VECTOR_BASE(name, type)                                                  \
    __VECTOR_DESTROY(name, type)                                               \
                                                                               \
    static inline name *name##_new_value(size_t n, type value) {               \
        name *vector = name##_new_size(n);                                     \
        if (vector == NULL) return NULL;                                       \
        memset(vector->a, (type)value, n * sizeof(type));                      \
        vector->n = n;                                                         \
        return vector;                                                         \
    }                                                                          \
                                                                               \
    static inline name *name##_new_ones(size_t n) {                            \
        return name##_new_value(n, (type)1);                                   \
    }                                                                          \
                                                                               \
    static inline name *name##_new_zeros(size_t n) {                           \
        return name##_new_value(n, (type)0);                                   \
    }                                                                          \
                                                                               \
    static inline void name##_set(name *vector, type value) {                  \
        size_t n = vector->n;                                                  \
        memset(vector->a, (type)value, n * sizeof(type));                      \
    }                                                                          \
                                                                               \
    static inline name *name##_copy(name *vector, size_t n) {                  \
        name *cpy = name##_new_size(n);                                        \
        memcpy(vector->a, cpy->a, n * sizeof(type));                           \
        cpy->n = n;                                                            \
        return cpy;                                                            \
    }                                                                          \
                                                                               \
    static inline type name##_max(name *vector, size_t n) {                    \
        type max_val = 0;                                                      \
        type val;                                                              \
        for (int i = 0; i < n; i++) {                                          \
            val = vector->a[i];                                                \
            if (val > max_val) max_val = val;                                  \
        }                                                                      \
        return max_val;                                                        \
    }                                                                          \
                                                                               \
    static inline type name##_min(name *vector, size_t n) {                    \
        if (n < 1) return (type) 0;                                            \
        type val = vector->a[0];                                               \
        type min_val = val;                                                    \
        for (int i = 1; i < n; i++) {                                          \
            val = vector->a[i];                                                \
            if (val < min_val) min_val = val;                                  \
        }                                                                      \
        return min_val;                                                        \
    }                                                                          \
                                                                               \
    static inline int64_t name##_argmax(name *vector, size_t n) {              \
        if (n < 1) return -1;                                                  \
        type max_val = 0;                                                      \
        int64_t argmax = 0;                                                    \
        type val;                                                              \
        for (int i = 0; i < n; i++) {                                          \
            val = vector->a[i];                                                \
            if (val > max_val) {                                               \
                max_val = val;                                                 \
                argmax = i;                                                    \
            }                                                                  \
        }                                                                      \
        return argmax;                                                         \
    }                                                                          \
                                                                               \
    static inline int64_t name##_argmin(name *vector, size_t n) {              \
        if (n < 1) return (type) -1;                                           \
        type val = vector->a[0];                                               \
        type min_val = val;                                                    \
        int64_t argmin = 0;                                                    \
        for (int i = 1; i < n; i++) {                                          \
            val = vector->a[i];                                                \
            if (val < min_val) {                                               \
                min_val = val;                                                 \
                argmin = i;                                                    \
            }                                                                  \
        }                                                                      \
        return argmin;                                                         \
    }                                                                          \
                                                                               \
    static inline void name##_add(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] += c;                                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_sub(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] -= c;                                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_mul(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] *= c;                                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_div(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] /= c;                                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_log(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] += log(vector->a[i]);                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_exp(name *vector, type c, size_t n) {            \
        for (int i = 0; i < n; i++) {                                          \
            vector->a[i] += exp(vector->a[i]);                                 \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type name##_sum(name *vector, size_t n) {                    \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += vector->a[i];                                            \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_product(name *vector, size_t n) {                \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result *= vector->a[i];                                            \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum(name *vector, size_t n) {                \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += log(vector->a[i]);                                       \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum_exp(name *vector, size_t n) {            \
        type max = name##_max(vector, n);                                      \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += exp(vector->a[i] - max);                                 \
        }                                                                      \
        return max + log(result);                                              \
    }                                                                          \
                                                                               \
    static inline void name##_add_vector(name *v1, name *v2, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            v1->a[i] += v2->a[i];                                              \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_sub_vector(name *v1, name *v2, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            v1->a[i] -= v2->a[i];                                              \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_mul_vector(name *v1, name *v2, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            v1->a[i] *= v2->a[i];                                              \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_div_vector(name *v1, name *v2, size_t n) {       \
        for (int i = 0; i < n; i++) {                                          \
            v1->a[i] /= v2->a[i];                                              \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type name##_dot(name *v1, name *v2, size_t n) {              \
        type result = 0;                                                       \
        for (int i = 0; i < n; i++) {                                          \
            result += v1->a[i] * v2->a[i];                                     \
        }                                                                      \
        return result;                                                         \
    }


#endif