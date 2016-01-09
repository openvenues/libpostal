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


bool sparse_matrix_add_unique_columns(sparse_matrix_t *matrix, khash_t(int_set) *unique_columns, uint32_array *array) {
    size_t n = matrix->indices->n;
    uint32_t *indices = matrix->indices->a;

    kh_clear(int_set, unique_columns);

    size_t i;

    for (i = 0; i < n; i++) {
        uint32_t col = indices[i];

        int ret;
        kh_put(int_set, unique_columns, (khint_t)col, &ret);
        if (ret < 0) {
            return false;
        }
    }

    uint32_array_clear(array);
    uint32_array_resize(array, kh_size(unique_columns));

    khint_t k;

    kh_foreach_key(unique_columns, k, {
        uint32_array_push(array, (uint32_t)k);
    })

    return true;
}

uint32_array *sparse_matrix_unique_columns(sparse_matrix_t *matrix) {
    khash_t(int_set) *unique_columns = kh_init(int_set);
    uint32_array *ret = uint32_array_new();

    if (sparse_matrix_add_unique_columns(matrix, unique_columns, ret)) {
        kh_destroy(int_set, unique_columns);
        return ret;
    }

    kh_destroy(int_set, unique_columns);
    uint32_array_destroy(ret);
    return NULL;
}
