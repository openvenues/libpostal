#include "sparse_matrix_utils.h"
#include "float_utils.h"
#include "matrix.h"


sparse_matrix_t *sparse_matrix_new_from_matrix(double_matrix_t *matrix) {
    sparse_matrix_t *sparse = sparse_matrix_new_shape(matrix->m, matrix->n);

    for (size_t i = 0; i < matrix->m; i++) {
        for (size_t j = 0; j < matrix->n; j++) {    
            double value = double_matrix_get(matrix, i, j);
            if (!double_equals(value, 0.0)) {
                sparse_matrix_append(sparse, j, value);
            }
        }
        sparse_matrix_finalize_row(sparse);
    }
    return sparse;
}


bool sparse_matrix_add_unique_columns(sparse_matrix_t *matrix, khash_t(int_uint32) *unique_columns, uint32_array *array) {
    size_t n = matrix->indices->n;
    uint32_t *indices = matrix->indices->a;

    kh_clear(int_uint32, unique_columns);

    size_t i;
    khiter_t k;

    for (i = 0; i < n; i++) {
        uint32_t col = indices[i];

        int ret = 0;
        k = kh_get(int_uint32, unique_columns, col);  
        if (k == kh_end(unique_columns)) {
            uint32_t next_id = (uint32_t)kh_size(unique_columns);

            k = kh_put(int_uint32, unique_columns, col, &ret);
            if (ret < 0) {
                return false;
            }
            kh_value(unique_columns, k) = next_id;
        }

    }

    uint32_array_clear(array);
    if (!uint32_array_resize_fixed(array, kh_size(unique_columns))) {
        return false;
    }

    khint_t key;

    uint32_t *batch = array->a;
    uint32_t col_id;

    kh_foreach(unique_columns, key, col_id, {
        batch[col_id] = (uint32_t)key;
    })

    return true;
}

bool sparse_matrix_alias_columns(sparse_matrix_t *matrix, khash_t(int_uint32) *unique_columns) {
    size_t n = matrix->indices->n;
    uint32_t *indices = matrix->indices->a;

    size_t i;
    khiter_t k;
    uint32_t col_id;

    for (i = 0; i < n; i++) {
        uint32_t col = indices[i];

        int ret = 0;
        k = kh_get(int_uint32, unique_columns, col);  
        if (k != kh_end(unique_columns)) {
            col_id = kh_value(unique_columns, k);
            indices[i] = col_id;
        } else {
            return false;
        }
    }

    matrix->n = kh_size(unique_columns);

    return true;
}

inline bool sparse_matrix_add_unique_columns_alias(sparse_matrix_t *matrix, khash_t(int_uint32) *unique_columns, uint32_array *array) {
    return sparse_matrix_add_unique_columns(matrix, unique_columns, array) &&
           sparse_matrix_alias_columns(matrix, unique_columns);
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
