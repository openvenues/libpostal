#ifndef SPARSE_MATRIX_UTILS_H
#define SPARSE_MATRIX_UTILS_H

#include <stdlib.h>

#include "sparse_matrix.h"
#include "matrix.h"

sparse_matrix_t *sparse_matrix_new_from_matrix(matrix_t *matrix);

#endif