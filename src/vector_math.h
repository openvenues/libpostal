#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

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