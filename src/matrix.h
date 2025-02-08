#ifndef MATRIX_H
#define MATRIX_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "collections.h"
#include "file_utils.h"
#include "vector.h"
#include "vector_math.h"

#ifdef HAVE_CBLAS
#include <cblas.h>
#else
#warning "No CBLAS"
#endif

typedef enum {
    MATRIX_DENSE,
    MATRIX_SPARSE
} matrix_type_t;

#define MATRIX_INIT(name, type, type_name, array_type)                                                          \
    typedef struct {                                                                                            \
        size_t m, n;                                                                                            \
        type *values;                                                                                           \
    } name##_t;                                                                                                 \
                                                                                                                \
    static name##_t *name##_new(size_t m, size_t n) {                                                           \
        name##_t *matrix = malloc(sizeof(name##_t));                                                \
                                                                                                                \
        if (matrix == NULL) {                                                                                   \
            return NULL;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        matrix->m = m;                                                                                          \
        matrix->n = n;                                                                                          \
                                                                                                                \
        matrix->values = malloc(sizeof(type) * m * n);                                                          \
        if (matrix->values == NULL) {                                                                           \
            free(matrix);                                                                                       \
            return NULL;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        return matrix;                                                                                          \
                                                                                                                \
    }                                                                                                           \
                                                                                                                \
    static name##_t *name##_new_aligned(size_t m, size_t n, size_t alignment) {                                 \
        name##_t *matrix = malloc(sizeof(name##_t));                                                            \
                                                                                                                \
        if (matrix == NULL) {                                                                                   \
            return NULL;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        matrix->m = m;                                                                                          \
        matrix->n = n;                                                                                          \
                                                                                                                \
        matrix->values = aligned_malloc(sizeof(type) * m * n, alignment);                                       \
        if (matrix->values == NULL) {                                                                           \
            free(matrix);                                                                                       \
            return NULL;                                                                                        \
        }                                                                                                       \
                                                                                                                \
        return matrix;                                                                                          \
                                                                                                                \
    }                                                                                                           \
                                                                                                                \
    static void name##_destroy(name##_t *self) {                                                                \
        if (self == NULL) return;                                                                               \
                                                                                                                \
        if (self->values != NULL) {                                                                             \
            free(self->values);                                                                                 \
        }                                                                                                       \
                                                                                                                \
        free(self);                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static void name##_destroy_aligned(name##_t *self) {                                                        \
        if (self == NULL) return;                                                                               \
                                                                                                                \
        if (self->values != NULL) {                                                                             \
            aligned_free(self->values);                                                                         \
        }                                                                                                       \
                                                                                                                \
        free(self);                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_zero(name##_t *self) {                                                            \
        memset(self->values, 0, self->m * self->n * sizeof(type));                                              \
    }                                                                                                           \
                                                                                                                \
                                                                                                                \
    static inline bool name##_resize(name##_t *self, size_t m, size_t n) {                                      \
        if (self == NULL) return false;                                                                         \
                                                                                                                \
        if (m * n > (self->m * self->n)) {                                                                      \
            type *ptr = realloc(self->values, sizeof(type) * m * n);                                            \
            if (ptr == NULL) {                                                                                  \
                return false;                                                                                   \
            }                                                                                                   \
            self->values = ptr;                                                                                 \
        }                                                                                                       \
                                                                                                                \
        self->m = m;                                                                                            \
        self->n = n;                                                                                            \
                                                                                                                \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_resize_aligned(name##_t *self, size_t m, size_t n, size_t alignment) {            \
        if (self == NULL) return false;                                                                         \
                                                                                                                \
        if (m * n > (self->m * self->n)) {                                                                      \
            type *ptr = aligned_resize(self->values, sizeof(type) * self->m * self->n, sizeof(type) * m * n, alignment); \
            if (ptr == NULL) {                                                                                  \
                return false;                                                                                   \
            }                                                                                                   \
            self->values = ptr;                                                                                 \
        }                                                                                                       \
                                                                                                                \
        self->m = m;                                                                                            \
        self->n = n;                                                                                            \
                                                                                                                \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_resize_fill_zeros(name##_t *self, size_t m, size_t n) {                           \
        size_t old_m = self->m;                                                                                 \
        bool ret = name##_resize(self, m, n);                                                                   \
        if (ret && m > old_m) {                                                                                 \
            memset(self->values + old_m, 0, (m - old_m) * self->n * sizeof(type));                              \
        }                                                                                                       \
        return ret;                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_resize_aligned_fill_zeros(name##_t *self, size_t m, size_t n, size_t alignment) { \
        size_t old_m = self->m;                                                                                 \
        bool ret = name##_resize_aligned(self, m, n, alignment);                                                \
        if (ret && m > old_m) {                                                                                 \
            memset(self->values + old_m, 0, (m - old_m) * self->n * sizeof(type));                              \
        }                                                                                                       \
        return ret;                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline name##_t *name##_new_copy(name##_t *self) {                                                   \
        name##_t *cpy = name##_new(self->m, self->n);                                                           \
        size_t num_values = self->m * self->n;                                                                  \
        memcpy(cpy->values, self->values, num_values * sizeof(type));                                           \
                                                                                                                \
        return cpy;                                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_copy(name##_t *self, name##_t *other) {                                           \
        if (self->m != other->m || self->n != other->n) {                                                       \
            return false;                                                                                       \
        }                                                                                                       \
        size_t num_values = self->m * self->n;                                                                  \
                                                                                                                \
        memcpy(other->values, self->values, num_values * sizeof(type));                                         \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_init_values(name##_t *self, type *values) {                                       \
        size_t num_values = self->m * self->n;                                                                  \
        memcpy(self->values, values, num_values * sizeof(type));                                                \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_set(name##_t *self, type value) {                                                 \
        array_type##_set(self->values, value, self->m * self->n);                                               \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_set_row(name##_t *self, size_t index, type *row) {                                \
        size_t offset = index * self->n;                                                                        \
        type *values = self->values;                                                                            \
        size_t n = self->n;                                                                                     \
        memcpy(values + offset, row, n * sizeof(type));                                                         \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_set_scalar(name##_t *self, size_t row_index, size_t col_index, type value) {      \
        size_t offset = row_index * self->n + col_index;                                                        \
        self->values[offset] = value;                                                                           \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_add_scalar(name##_t *self, size_t row_index, size_t col_index, type value) {      \
        size_t offset = row_index * self->n + col_index;                                                        \
        self->values[offset] += value;                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_sub_scalar(name##_t *self, size_t row_index, size_t col_index, type value) {      \
        size_t offset = row_index * self->n + col_index;                                                        \
        self->values[offset] -= value;                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_mul_scalar(name##_t *self, size_t row_index, size_t col_index, type value) {      \
        size_t offset = row_index * self->n + col_index;                                                        \
        self->values[offset] *= value;                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_div_scalar(name##_t *self, size_t row_index, size_t col_index, type value) {      \
        size_t offset = row_index * self->n + col_index;                                                        \
        self->values[offset] /= value;                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline type name##_get(name##_t *self, size_t row_index, size_t col_index) {                         \
        size_t index = row_index * self->n + col_index;                                                         \
        return self->values[index];                                                                             \
    }                                                                                                           \
                                                                                                                \
    static inline type *name##_get_row(name##_t *self, size_t row_index) {                                      \
        size_t index = row_index * self->n;                                                                     \
        return self->values + index;                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline name##_t *name##_new_value(size_t m, size_t n, type value) {                                  \
        name##_t *matrix = name##_new(m, n);                                                                    \
        name##_set(matrix, value);                                                                              \
        return matrix;                                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline name##_t *name##_new_zeros(size_t m, size_t n) {                                              \
        name##_t *matrix = name##_new(m, n);                                                                    \
        name##_zero(matrix);                                                                                    \
        return matrix;                                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline name##_t *name##_new_ones(size_t m, size_t n) {                                               \
        return name##_new_value(m, n, (type)1);                                                                 \
    }                                                                                                           \
                                                                                                                \
    static inline name##_t *name##_new_values(size_t m, size_t n, type *values) {                               \
        name##_t *matrix = name##_new(m, n);                                                                    \
        memcpy(matrix->values, values, m * n * sizeof(type));                                                   \
        return matrix;                                                                                          \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_div(name##_t *self, type value) {                                                 \
        array_type##_div(self->values, value, self->m * self->n);                                               \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_div_matrix(name##_t *self, name##_t *other) {                                     \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_div_array(self->values, other->values, self->m * self->n);                                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_div_matrix_times_scalar(name##_t *self, name##_t *other, type v) {                \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_div_array_times_scalar(self->values, other->values, v, self->m * self->n);                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_mul(name##_t *self, type value) {                                                 \
        array_type##_mul(self->values, value, self->m * self->n);                                               \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_mul_matrix(name##_t *self, name##_t *other) {                                     \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_mul_array(self->values, other->values, self->m * self->n);                                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_mul_matrix_times_scalar(name##_t *self, name##_t *other, type v) {                \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_mul_array_times_scalar(self->values, other->values, v, self->m * self->n);                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_add(name##_t *self, type value) {                                                 \
        array_type##_add(self->values, self->m * self->n, value);                                               \
    }                                                                                                           \
                                                                                                                \
                                                                                                                \
    static inline bool name##_add_matrix(name##_t *self, name##_t *other) {                                     \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_add_array(self->values, other->values, self->m * self->n);                                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_add_matrix_times_scalar(name##_t *self, name##_t *other, type v) {                \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_add_array_times_scalar(self->values, other->values, v, self->m * self->n);                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline void name##_sub(name##_t *self, type value) {                                                 \
        array_type##_sub(self->values, value, self->m * self->n);                                               \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_sub_matrix(name##_t *self, name##_t *other) {                                     \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_sub_array(self->values, other->values, self->m * self->n);                                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static inline bool name##_sub_matrix_times_scalar(name##_t *self, name##_t *other, type v) {                \
        if (self->m != other->m || self->n != other->n) return false;                                           \
        array_type##_sub_array_times_scalar(self->values, other->values, v, self->m * self->n);                 \
        return true;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static name##_t *name##_read(FILE *f) {                                                                     \
        name##_t *mat = malloc(sizeof(name##_t));                                                               \
        if (mat == NULL) return NULL;                                                                           \
                                                                                                                \
        mat->values = NULL;                                                                                     \
                                                                                                                \
        uint64_t m = 0;                                                                                         \
        uint64_t n = 0;                                                                                         \
                                                                                                                \
        if (!file_read_uint64(f, &m) ||                                                                         \
            !file_read_uint64(f, &n)) {                                                                         \
            goto exit_##name##_allocated;                                                                       \
        }                                                                                                       \
                                                                                                                \
        mat->m = (size_t)m;                                                                                     \
        mat->n = (size_t)n;                                                                                     \
                                                                                                                \
        size_t len_data = mat->m * mat->n;                                                                      \
                                                                                                                \
        type *data = malloc(len_data * sizeof(type));                                                           \
        if (data == NULL) {                                                                                     \
            log_error("error in data malloc\n");                                                                \
            goto exit_##name##_allocated;                                                                       \
        }                                                                                                       \
                                                                                                                \
        if (!file_read_##array_type(f, data, len_data)) {                                                       \
            free(data);                                                                                         \
            goto exit_##name##_allocated;                                                                       \
        }                                                                                                       \
                                                                                                                \
        mat->values = data;                                                                                     \
                                                                                                                \
        return mat;                                                                                             \
                                                                                                                \
    exit_##name##_allocated:                                                                                    \
        name##_destroy(mat);                                                                                    \
        return NULL;                                                                                            \
    }                                                                                                           \
                                                                                                                \
    static bool name##_write(name##_t *self, FILE *f) {                                                         \
        if (self == NULL || self->values == NULL) {                                                             \
            return false;                                                                                       \
        }                                                                                                       \
                                                                                                                \
        if (!file_write_uint64(f, (uint64_t)self->m) ||                                                         \
            !file_write_uint64(f, (uint64_t)self->n)) {                                                         \
            return false;                                                                                       \
        }                                                                                                       \
                                                                                                                \
        uint64_t len_data = (uint64_t)self->m * (uint64_t)self->n;                                              \
                                                                                                                \
        for (uint64_t i = 0; i < len_data; i++) {                                                               \
            if (!file_write_##type_name(f, self->values[i])) {                                                  \
                return false;                                                                                   \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        return true;                                                                                            \
    }


#define MATRIX_INIT_FLOAT_BASE(name, type, type_name, array_type)                       \
    MATRIX_INIT(name, type, type_name, array_type)                                      \
                                                                                        \
    static inline void name##_log(name##_t *self) {                                     \
        array_type##_log(self->values, self->m * self->n);                              \
    }                                                                                   \
                                                                                        \
    static inline void name##_exp(name##_t *self) {                                     \
        array_type##_exp(self->values, self->m * self->n);                              \
    }                                                                                   \
                                                                                        \
    static inline void name##_dot_vector(name##_t *self, type *vec, type *result) {     \
        type *values = self->values;                                                    \
        size_t m = self->m;                                                             \
        size_t n = self->n;                                                             \
        for (size_t i = 0; i < m; i++) {                                                \
            for (size_t j = 0; j < n; j++) {                                            \
                result[i] += values[n * i + j] * vec[j];                                \
            }                                                                           \
        }                                                                               \
    }


#ifdef HAVE_CBLAS
#define MATRIX_INIT_FLOAT(name, type, type_name, array_type, blas_prefix)                                   \
    MATRIX_INIT_FLOAT_BASE(name, type, type_name, array_type)                                               \
                                                                                                            \
    static inline bool name##_dot_matrix(name##_t *m1, name##_t *m2, name##_t *result) {                    \
        if (m1->n != m2->m || m1->m != result->m || m2->n != result->n) {                                   \
            return false;                                                                                   \
        }                                                                                                   \
                                                                                                            \
        log_debug("doing CBLAS\n");                                                                         \
        cblas_##blas_prefix##gemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,                                \
                    m1->m, m2->n, m1->n, 1.0,                                                               \
                    m1->values, m1->n,                                                                      \
                    m2->values, m2->n, 0.0,                                                                 \
                    result->values, result->n                                                               \
                    );                                                                                      \
                                                                                                            \
        return true;                                                                                        \
    }

#else
#define MATRIX_INIT_FLOAT(name, type, type_name, array_type, blas_prefix)                                       \
    MATRIX_INIT_FLOAT_BASE(name, type, type_name, array_type)                                                   \
                                                                                                                \
    static inline bool name##_dot_matrix(name##_t *m1, name##_t *m2, name##_t *result) {                        \
        if (m1->n != m2->m || m1->m != result->m || m2->n != result->n) {                                       \
            return false;                                                                                       \
        }                                                                                                       \
                                                                                                                \
        size_t m1_rows = m1->m;                                                                                 \
        size_t m1_cols = m1->n;                                                                                 \
        size_t m2_rows = m2->m;                                                                                 \
        size_t m2_cols = m2->n;                                                                                 \
                                                                                                                \
        type *m1_values = m1->values;                                                                           \
        type *m2_values = m2->values;                                                                           \
        type *result_values = result->values;                                                                   \
                                                                                                                \
        for (size_t i = 0; i < m1_rows; i++) {                                                                  \
            for (size_t j = 0; j < m2_cols; j++) {                                                              \
                size_t result_index = m2_cols * i + j;                                                          \
                result_values[result_index] = 0.0;                                                              \
                for (size_t k = 0; k < m2_rows; k++) {                                                          \
                    result_values[result_index] += m1_values[m1_cols * i + k] * m2_values[m2_cols * k + j];     \
                }                                                                                               \
            }                                                                                                   \
        }                                                                                                       \
                                                                                                                \
        return true;                                                                                            \
    }
#endif

MATRIX_INIT(uint32_matrix, uint32_t, uint32, uint32_array)

MATRIX_INIT_FLOAT(float_matrix, float, float, float_array,s)
MATRIX_INIT_FLOAT(double_matrix, double, double, double_array,d)


#endif
