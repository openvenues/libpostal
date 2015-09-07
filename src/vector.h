#ifndef VECTOR_H
#define VECTOR_H

 

#include <stdlib.h>

#include "klib/kvec.h"

// Wrapper around kvec.h to provide dynamically allocated vectors
#define __VECTOR_BASE(name, type)  typedef kvec_t(type) name;               \
    static inline name *name##_new(void) {                                  \
        name *array = malloc(sizeof(name));                                 \
        if (array == NULL) return NULL;                                     \
        kv_init(*array);                                                    \
        return array;                                                       \
    }                                                                       \
    static inline name *name##_new_size(size_t size) {                      \
        name *array = name##_new();                                         \
        kv_resize(type, *array, size);                                      \
        return array;                                                       \
    }                                                                       \
    static inline void name##_push(name *array, type value) {               \
        kv_push(type, *array, value);                                       \
    }                                                                       \
    static inline void name##_extend(name *array, name *other) {            \
        for (int i = 0; i < other->n; i++) {                                \
            kv_push(type, *array, *(other->a + i));                         \
        }                                                                   \
    }                                                                       \
    static inline type name##_pop(name *array) {                            \
        return kv_pop(*array);                                              \
    }                                                                       \
    static inline void name##_clear(name *array) {                          \
        kv_clear(*array);                                                   \
    }                                                                       \
    static inline void name##_resize(name *array, size_t size) {            \
        kv_resize(type, *array, size);                                      \
    }                                                                          \
                                                                               \
    static inline void *name##_copy(name *dst, name *src, size_t n) {          \
        if (dst->m < n) name##_resize(dst, n);                                 \
        memcpy(dst->a, src->a, n * sizeof(type));                              \
        dst->n = n;                                                            \
    }                                                                          \
                                                                               \
    static inline name *name##_new_copy(name *vector, size_t n) {              \
        name *cpy = name##_new_size(n);                                        \
        name##_copy(cpy, vector, n);                                           \
        return cpy;                                                            \
    }                                                                          \
                                                                               \


#define __VECTOR_DESTROY(name, type)                                    \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        kv_destroy(*array);                                             \
        free(array);                                                    \
    }                                                                   


#define __VECTOR_DESTROY_FREE_DATA(name, type, free_func)               \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        for (int i = 0; i < array->n; i++) {                            \
            free_func(array->a[i]);                                     \
        }                                                               \
        kv_destroy(*array);                                             \
        free(array);                                                    \
    }                                                                   

#define VECTOR_INIT(name, type)                                         \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY(name, type)                                      

#define VECTOR_INIT_FREE_DATA(name, type, free_func)                    \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY_FREE_DATA(name, type, free_func)                 


 

#endif
