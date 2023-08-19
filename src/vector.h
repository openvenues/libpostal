#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>

#define DEFAULT_VECTOR_SIZE 8

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#include <malloc.h>
static inline void *aligned_malloc(size_t size, size_t alignment) {
    return _aligned_malloc(size, alignment);
}
static inline void *aligned_resize(void *p, size_t old_size, size_t new_size, size_t alignment) {
    return _aligned_realloc(p, new_size, alignment);
}
static inline void aligned_free(void *p) {
    _aligned_free(p);
}
#else
#include <stdlib.h>
static inline void *aligned_malloc(size_t size, size_t alignment)
{
    void *p;
    int ret = posix_memalign(&p, alignment, size);
    return (ret == 0) ? p : NULL;
}
static inline void *aligned_resize(void *p, size_t old_size, size_t new_size, size_t alignment)
{
    if ((alignment == 0) || ((alignment & (alignment - 1)) != 0) || (alignment < sizeof(void *))) {
        return NULL;
    }

    if (p == NULL) {
        return NULL;
    }

    void *p1 = aligned_malloc(new_size, alignment);
    if (p1 == NULL) {
        free(p);
        return NULL;
    }

    memcpy(p1, p, old_size);
    free(p);
    return p1;
}
static inline void aligned_free(void *p)
{
    free(p);
}
#endif

#ifdef _MSC_VER
#define MIE_ALIGN(x) __declspec(align(x))
#else
#define MIE_ALIGN(x) __attribute__((aligned(x)))
#endif

// Based kvec.h, dynamic vectors of any type
#define __VECTOR_BASE(name, type) typedef struct { size_t n, m; type *a; } name;            \
    static inline name *name##_new_size(size_t size) {                                      \
        name *array = malloc(sizeof(name));                                                 \
        if (array == NULL) return NULL;                                                     \
        array->n = array->m = 0;                                                            \
        array->a = malloc((size > 0 ? size : 1) * sizeof(type));                            \
        if (array->a == NULL) return NULL;                                                  \
        array->m = size;                                                                    \
        return array;                                                                       \
    }                                                                                       \
    static inline name *name##_new(void) {                                                  \
        return name##_new_size(DEFAULT_VECTOR_SIZE);                                        \
    }                                                                                       \
    static inline name *name##_new_size_fixed(size_t size) {                                \
        name *array = name##_new_size(size);                                                \
        if (array == NULL) return NULL;                                                     \
        array->n = size;                                                                    \
        return array;                                                                       \
    }                                                                                       \
    static inline name *name##_new_aligned(size_t size, size_t alignment) {                 \
        name *array = malloc(sizeof(name));                                                 \
        if (array == NULL) return NULL;                                                     \
        array->n = array->m = 0;                                                            \
        array->a = aligned_malloc(size * sizeof(type), alignment);                          \
        if (array->a == NULL) return NULL;                                                  \
        array->m = size;                                                                    \
        return array;                                                                       \
    }                                                                                       \
    static inline bool name##_resize(name *array, size_t size) {                            \
        if (size <= array->m)return true;                                                   \
        type *ptr = realloc(array->a, sizeof(type) * size);                                 \
        if (ptr == NULL) return false;                                                      \
        array->a = ptr;                                                                     \
        array->m = size;                                                                    \
        return true;                                                                        \
    }                                                                                       \
    static inline bool name##_resize_aligned(name *array, size_t size, size_t alignment) {  \
        if (size <= array->m) return true;                                                  \
        type *ptr = aligned_resize(array->a, sizeof(type) * array->m, sizeof(type) * size, alignment); \
        if (ptr == NULL) return false;                                                      \
        array->a = ptr;                                                                     \
        array->m = size;                                                                    \
        return true;                                                                        \
    }                                                                                       \
    static inline bool name##_resize_fixed(name *array, size_t size) {                      \
        if (!name##_resize(array, size)) return false;                                      \
        array->n = size;                                                                    \
        return true;                                                                        \
    }                                                                                       \
    static inline bool name##_resize_fixed_aligned(name *array, size_t size, size_t alignment) {  \
        if (!name##_resize_aligned(array, size, alignment)) return false;                   \
        array->n = size;                                                                    \
        return true;                                                                        \
    }                                                                                       \
    static inline void name##_push(name *array, type value) {                               \
        if (array->n == array->m) {                                                         \
            size_t size = array->m ? array->m << 1 : 2;                                     \
            type *ptr = realloc(array->a, sizeof(type) * size);                             \
            if (ptr == NULL) {                                                              \
                fprintf(stderr, "realloc failed during " #name "_push\n");                  \
                exit(EXIT_FAILURE);                                                         \
            }                                                                               \
            array->a = ptr;                                                                 \
            array->m = size;                                                                \
        }                                                                                   \
        array->a[array->n++] = value;                                                       \
    }                                                                                       \
    static inline bool name##_extend(name *array, name *other) {                            \
        bool ret = false;                                                                   \
        size_t new_size = array->n + other->n;                                              \
        if (new_size > array->m) ret = name##_resize(array, new_size);                      \
        if (!ret) return false;                                                             \
        memcpy(array->a + array->n, other->a, other->n * sizeof(type));                     \
        array->n = new_size;                                                                \
        return ret;                                                                         \
    }                                                                                       \
    static inline void name##_pop(name *array) {                                            \
        if (array->n > 0) array->n--;                                                       \
    }                                                                                       \
    static inline void name##_clear(name *array) {                                          \
        array->n = 0;                                                                       \
    }                                                                                       \
    static inline bool name##_copy(name *dst, name *src, size_t n) {                        \
        bool ret = true;                                                                    \
        if (dst->m < n) ret = name##_resize(dst, n);                                        \
        if (!ret) return false;                                                             \
        memcpy(dst->a, src->a, n * sizeof(type));                                           \
        dst->n = n;                                                                         \
        return ret;                                                                         \
    }                                                                                       \
    static inline name *name##_new_copy(name *vector, size_t n) {                           \
        name *cpy = name##_new_size(n);                                                     \
        if (!name##_copy(cpy, vector, n)) return NULL;                                      \
        return cpy;                                                                         \
    }

#define __VECTOR_DESTROY(name, type)                                    \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        if (array->a != NULL) free(array->a);                           \
        free(array);                                                    \
    }                                                                   \
    static inline void name##_destroy_aligned(name *array) {            \
        if (array == NULL) return;                                      \
        if (array->a != NULL) aligned_free(array->a);                   \
        free(array);                                                    \
    }

#define __VECTOR_DESTROY_FREE_DATA(name, type, free_func)               \
    static inline void name##_destroy(name *array) {                    \
        if (array == NULL) return;                                      \
        if (array->a != NULL) {                                         \
            for (size_t i = 0; i < array->n; i++) {                     \
                free_func(array->a[i]);                                 \
            }                                                           \
        }                                                               \
        free(array->a);                                                 \
        free(array);                                                    \
    }                                                                   \
    static inline void name##_destroy_aligned(name *array) {            \
        if (array == NULL) return;                                      \
        if (array->a != NULL) {                                         \
            for (size_t i = 0; i < array->n; i++) {                     \
                free_func(array->a[i]);                                 \
            }                                                           \
        }                                                               \
        aligned_free(array->a);                                        \
        free(array);                                                    \
    }

#define VECTOR_INIT(name, type)                                         \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY(name, type)                                      

#define VECTOR_INIT_FREE_DATA(name, type, free_func)                    \
    __VECTOR_BASE(name, type)                                           \
    __VECTOR_DESTROY_FREE_DATA(name, type, free_func)                 

#endif
