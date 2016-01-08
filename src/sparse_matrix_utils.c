#include "sparse_matrix_utils.h"
#include "float_utils.h"
#include "matrix.h"


sparse_matrix_t *sparse_matrix_new_from_matrix(matrix_t *matrix) {
    sparse_matrix_t *sparse = sparse_matrix_new_shape(matrix->m, matrix->n);

    for (size_t i = 0; i < matrix->m; i++) {
        for (size_t j = 0; j < matrix->n; j++) {    
            double value = matrix_get(matrix, i, j);
            if (!double_equals(value, 0.0)) {
                sparse_matrix_append(sparse, j, value);
            }
        }
        sparse_matrix_finalize_row(sparse);
    }
    return sparse;
}