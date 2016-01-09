#ifndef SPARSE_MATRIX_UTILS_H
#define SPARSE_MATRIX_UTILS_H

#include <stdlib.h>

#include "sparse_matrix.h"
#include "matrix.h"

sparse_matrix_t *sparse_matrix_new_from_matrix(matrix_t *matrix);
uint32_array *sparse_matrix_unique_columns(sparse_matrix_t *matrix);
bool sparse_matrix_add_unique_columns(sparse_matrix_t *matrix, khash_t(int_set) *unique_columns, uint32_array *array);

#endif