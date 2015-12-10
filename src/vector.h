#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define DEFAULT_VECTOR_SIZE 8

// Based kvec.h, dynamic vectors of any type
#define __VECTOR_BASE(name, type) typedef struct { size_t n, m; type *a; } name;    \
    static inline name *name##_new_size(size_t size) {                              \
        name *array = malloc(sizeof(name));                                         \
        if (array == NULL) return NULL;                                             \
        array->n = array->m = 0;                                                    \
        array->a = NULL;                                                            \
        array->a = malloc(size * sizeof(type));                                     \
        if (array->a == NULL) return NULL;                                          \
        array->m = size;                                                            \
        return array;                                                               \
    }                                                                               \
    static inline name *name##_new(void) {                                          \
        return name##_new_size(DEFAULT_VECTOR_SIZE);                                \
    }                                                                               \
    static inline void name##_resize(name *array, size_t size) {                    \
        if (size <= array->m) return;                                               \
        type *ptr = realloc(array->a, sizeof(type) * size);                         \
        if (ptr == NULL) return;                                                    \
        array->a = ptr;                                                             \
        array->m = size;                                                            \
    }                                                                               \
    static inline void name##_push(name *array, type value) {                       \
        if (array->n == array->m) {                                                 \
            size_t size = array->m ? array->m << 1 : 2;                             \
            type *ptr = realloc(array->a, sizeof(type) * size);                     \
            if (ptr == NULL) return;                                                \
            array->a = ptr;                                                         \
            array->m = size;                                                        \
        }                                                                           \
        array->a[array->n++] = value;                                               \
    }                                                                               \
    static inline void name##_extend(name *array, name *other) {                    \
        size_t new_size = array->n + other->n;                                      \
        if (new_size >= array->m) name##_resize(array, new_size);                   \
        memcpy(array->a + array->n, other->a, other->n * sizeof(type));             \
        array->n = new_size;                                                        \
    }                                                                               \
    static inline type name##_pop(name *array) {                                    \
        return array->a[--array->n];                                                \
    }                                                                               \
    static inline void name##_clear(name *array) {                                  \
        array->n = 0;                                                               \
    }                                                                               \
    static inline void name##_copy(name *dst, name *src, size_t n) {                \
        if (dst->m < n) name##_resize(dst, n);                                      \
        memcpy(dst->a, src->a, n * sizeof(type));                                   \
        dst->n = n;                                                                 \
    }                                                                               \
    static inline name *name##_new_copy(name *vector, size_t n) {                   \
        name *cpy = name##_new_size(n);                                             \
        name##_copy(cpy, vector, n);                                                \
        return cpy;                                                                 \
    }


#define __VECTOR_DESTROY(name, type)                                    \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        if (array->a != NULL) free(array->a);                           \
        free(array);                                                    \
    }                                                                   


#define __VECTOR_DESTROY_FREE_DATA(name, type, free_func)               \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        if (array->a != NULL) {                                         \
            for (int i = 0; i < array->n; i++) {                        \
                free_func(array->a[i]);                                 \
            }                                                           \
        }                                                               \
        free(array->a);                                                 \
        free(array);                                                    \
    }                                                                   

#define VECTOR_INIT(name, type)                                         \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY(name, type)                                      

#define VECTOR_INIT_FREE_DATA(name, type, free_func)                    \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY_FREE_DATA(name, type, free_func)                 


 

#endif
