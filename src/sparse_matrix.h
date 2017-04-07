/*
sparse_matrix.h
---------------

Double-valued dynamic compressed sparse row (CSR) sparse matrix.

A sparse matrix is a matrix where an overwhelming number of the
entries are 0. Thus we get significant space/time advantages
from only storing the non-zero values.

These types of matrices arise often when representing graphs
and term-document matrices in text collections.

The compressed sparse row format stores the following 3x4
dense matrix:

{   1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 2.0  }

with the following 3 arrays:

indptr = { 0, 1, 2, 3 }
indices = { 0, 1, 3 }
data = { 1.0, 1.0, 2.0 }

For a given row i, the indices indptr[i] through indptr[i+1]
denotes the number of nonzero columns in row i. The column
indices can be found at that contiguous location in indices
and the data values can be found at the same location in the
data array.

Sparse matrix row iteration, row indexing, scalar arithmetic
and dot products with vectors and dense matrices are efficient.
*/

#ifndef SPARSE_MATRIX_H
#define SPARSE_MATRIX_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "collections.h"
#include "file_utils.h"
#include "matrix.h"
#include "vector.h"

typedef struct {
    uint32_t m;
    uint32_t n;
    uint32_array *indptr;
    uint32_array *indices;
    double_array *data;
} sparse_matrix_t;


sparse_matrix_t *sparse_matrix_new(void);
sparse_matrix_t *sparse_matrix_new_shape(size_t m, size_t n);
void sparse_matrix_destroy(sparse_matrix_t *self);

void sparse_matrix_clear(sparse_matrix_t *self);

void sparse_matrix_append(sparse_matrix_t *self, uint32_t col, double val);
void sparse_matrix_append_row(sparse_matrix_t *self, uint32_t *col, double *val, size_t n);

void sparse_matrix_finalize_row(sparse_matrix_t *self);

void sparse_matrix_sort_indices(sparse_matrix_t *self);

int sparse_matrix_dot_vector(sparse_matrix_t *self, double *vec, size_t n, double *result);
int sparse_matrix_rows_dot_vector(sparse_matrix_t *self, uint32_t *rows, size_t m, double *vec, size_t n, double *result);

// No need to allocate vector. Equivalent to doing a dot product with a vector of all ones
int sparse_matrix_sum_cols(sparse_matrix_t *self, double *result, size_t n);
int sparse_matrix_rows_sum_cols(sparse_matrix_t *self, uint32_t *rows, size_t m, double *result, size_t n);

int sparse_matrix_sum_all_rows(sparse_matrix_t *self, double *result, size_t n);
int sparse_matrix_sum_rows(sparse_matrix_t *self, uint32_t *rows, size_t m, double *result, size_t n);

int sparse_matrix_dot_dense(sparse_matrix_t *self, double_matrix_t *matrix, double_matrix_t *result);
int sparse_matrix_dot_sparse(sparse_matrix_t *self, sparse_matrix_t *other, double_matrix_t *result);

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
