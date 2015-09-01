#ifndef SPARSE_MATRIX_H
#define SPARSE_MATRIX_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "collections.h"
#include "file_utils.h"
#include "matrix.h"
#include "vector.h"

// Simple, partial sparse matrix implementation


// Double-valued dynamic compressed sparse row (CSR) sparse matrix

typedef struct {
    uint32_t m;
    uint32_t n;
    uint32_array *indptr;
    uint32_array *indices;
    double_array *data;
} sparse_matrix_t;


sparse_matrix_t *sparse_matrix_new(void);
void sparse_matrix_destroy(sparse_matrix_t *self);

void sparse_matrix_clear(sparse_matrix_t *self);

void sparse_matrix_append(sparse_matrix_t *self, uint32_t col, double val);
void sparse_matrix_append_row(sparse_matrix_t *self, uint32_t *col, double *val, size_t n);

void sparse_matrix_finalize_row(sparse_matrix_t *self);

void sparse_matrix_sort_indices(sparse_matrix_t *self);

int sparse_matrix_dot_vector(sparse_matrix_t *self, double *vec, size_t n, double *result);
int sparse_matrix_rows_dot_vector(sparse_matrix_t *self, uint32_t *rows, size_t m, double *vec, size_t n, double *result);

int sparse_matrix_dot_dense(sparse_matrix_t *self, matrix_t *matrix, matrix_t *result);

bool sparse_matrix_write(sparse_matrix_t *self, FILE *f);
sparse_matrix_t *sparse_matrix_read(FILE *f);

#define sparse_matrix_foreach_row(sp, row_var, index_var, length_var, code) {   \
    uint32_t _row_start = 0, _row_end = 0;                                      \
    uint32_t *_indptr = sp->indptr->a;                                          \
    size_t _m = sp->m;                                                          \
                                                                                \
    for (uint32_t _i = 0; _i < _m; _i++) {                                      \
        (row_var) = _i;                                                         \
        _row_start = _indptr[_i];                                               \
        _row_end = _indptr[_i + 1];                                             \
        (index_var) = _row_start;                                               \
        (length_var) = _row_end - _row_start;                                   \
        code;                                                                   \
    }                                                                           \
}

#define sparse_matrix_foreach(sp, row_var, col_var, data_var, code) {           \
    uint32_t *_indices = sp->indices->a;                                        \
    double *_data = sp->data->a;                                                \
    uint32_t _index, _length;                                                   \
    sparse_matrix_foreach_row(sp, row_var, _index, _length, {                   \
        for (uint32_t _j = _index; _j < _index + _length; _j++) {               \
            (col_var) = _indices[_j];                                           \
            (data_var) = _data[_j];                                             \
            code;                                                               \
        }                                                                       \
    })                                                                          \
}   
    

#endif
