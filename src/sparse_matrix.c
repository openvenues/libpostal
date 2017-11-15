#include "sparse_matrix.h"
#include "klib/ksort.h"

sparse_matrix_t *sparse_matrix_new_shape(size_t m, size_t n) {
    sparse_matrix_t *matrix = calloc(1, sizeof(sparse_matrix_t));
    if (matrix == NULL) return NULL;
    matrix->m = m;
    matrix->n = n;
    matrix->indptr = uint32_array_new_size(m + 1);
    if (matrix->indptr == NULL) {
        goto exit_sparse_matrix_created;
    }
    uint32_array_push(matrix->indptr, 0);

    matrix->indices = uint32_array_new();
    if (matrix->indices == NULL) {
        goto exit_sparse_matrix_created;
    }

    matrix->data = double_array_new();
    if (matrix->data == NULL) {
        goto exit_sparse_matrix_created;
    }

    return matrix;

exit_sparse_matrix_created:
    sparse_matrix_destroy(matrix);
    return NULL;
}

sparse_matrix_t *sparse_matrix_new(void) {
    return sparse_matrix_new_shape(0, 0);
}


void sparse_matrix_destroy(sparse_matrix_t *self) {
    if (self == NULL) return;

    if (self->indptr != NULL) {
        uint32_array_destroy(self->indptr);
    }

    if (self->indices != NULL) {
        uint32_array_destroy(self->indices);
    }

    if (self->data != NULL) {
        double_array_destroy(self->data);
    }

    free(self);
}

inline void sparse_matrix_clear(sparse_matrix_t *self) {
    uint32_array_clear(self->indptr);
    uint32_array_push(self->indptr, 0);

    uint32_array_clear(self->indices);
    double_array_clear(self->data);
}

inline void sparse_matrix_finalize_row(sparse_matrix_t *self) {
    uint32_array_push(self->indptr, (uint32_t)self->indices->n);
    if (self->indptr->n > self->m + 1) {
        self->m++;
    }
}

inline void sparse_matrix_append(sparse_matrix_t *self, uint32_t col, double val) {
    uint32_array_push(self->indices, col);
    double_array_push(self->data, val);
    if (col >= self->n) self->n = col + 1;
}

inline void sparse_matrix_append_row(sparse_matrix_t *self, uint32_t *col, double *val, size_t n) {
    for (int i = 0; i < n; i++) {
        sparse_matrix_append(self, col[i], val[i]);
    }
    sparse_matrix_finalize_row(self);
}

typedef struct column_value {
    uint32_t col;
    double val;
} column_value_t;

VECTOR_INIT(column_value_array, column_value_t)

#define ks_lt_column_value(a, b) ((a).col < (b).col)

KSORT_INIT(column_value_array, column_value_t, ks_lt_column_value)

void sparse_matrix_sort_indices(sparse_matrix_t *self) {
    uint32_t row, row_start, row_len, i;

    column_value_array *col_vals = column_value_array_new();

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        for (i = row_start; i < row_start + row_len; i++) {
            column_value_array_push(col_vals, (column_value_t){self->indices->a[i], self->data->a[i]});
        }
        ks_introsort(column_value_array, col_vals->n, col_vals->a);

        for (i = 0; i < col_vals->n; i++) {
            column_value_t col_val = col_vals->a[i];
            self->indices->a[row_start + i] = col_val.col;
            self->data->a[row_start + i] = col_val.val;
        }
    })

}


inline int sparse_matrix_dot_vector(sparse_matrix_t *self, double *vec, size_t n, double *result) {
    if (n != self->n) return -1;

    uint32_t row, row_start, row_len;
    double val;
    double *data = self->data->a;

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        double sum = result[row];
        for (uint32_t col = row_start; col < row_start + row_len; col++) {
            sum += data[col] * vec[col];
        }
        result[row] = sum;
    })
    return 0;
}

int sparse_matrix_rows_dot_vector(sparse_matrix_t *self, uint32_t *rows, size_t m, double *vec, size_t n, double *result) {
    if (n != self->n) return -1;

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;
    double *data = self->data->a;

    for (int i = 0; i < m; i++) {
        uint32_t row = rows[i];

        double sum = result[i];
        if (row >= self->m) return -1;

        for (int j = indptr[row]; j < indptr[row+1]; j++) {
            sum += data[j] * vec[indices[j]];
        }

        result[i] = sum;

    }
    return 0;
}

int sparse_matrix_sum_cols(sparse_matrix_t *self, double *result, size_t n) {
    if (n != self->m) return -1;

    uint32_t row, row_start, row_len;
    double val;
    double *data = self->data->a;

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        double sum = result[row];
        for (uint32_t col = row_start; col < row_start + row_len; col++) {
            sum += data[col];
        }
        result[row] = sum;
    })
    return 0;

}

// No need to allocate actual vector for values, sum rather than a dot product
int sparse_matrix_rows_sum_cols(sparse_matrix_t *self, uint32_t *rows, size_t m, double *result, size_t n) {
    if (m != n) return -1;

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;
    double *data = self->data->a;

    for (int i = 0; i < m; i++) {
        uint32_t row = rows[i];

        double sum = result[i];
        if (row >= self->m) return -1;

        for (int j = indptr[row]; j < indptr[row+1]; j++) {
            sum += data[j];
        }

        result[i] = sum;
    }
    return 0;
}


int sparse_matrix_sum_all_rows(sparse_matrix_t *self, double *result, size_t n) {
    if (n != self->n) return -1;

    uint32_t row, row_start, row_len;
    double val;
    double *data = self->data->a;

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        for (uint32_t col = row_start; col < row_start + row_len; col++) {
            result[col] += data[col];
        }
    })
    return 0;

}

int sparse_matrix_sum_rows(sparse_matrix_t *self, uint32_t *rows, size_t m, double *result, size_t n) {
    if (n != self->n) return -1;

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;
    double *data = self->data->a;

    for (int i = 0; i < m; i++) {
        uint32_t row = rows[i];

        if (row >= self->m) return -1;

        for (int j = indptr[row]; j < indptr[row+1]; j++) {
            result[j] += data[j];
        }
    }
    return 0;
}



int sparse_matrix_dot_dense(sparse_matrix_t *self, double_matrix_t *matrix, double_matrix_t *result) {
    if (self->n != matrix->m || self->m != result->m || matrix->n != result->n) {
        return -1;
    }

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;
    double *data = self->data->a;

    size_t m1_rows = self->m;
    size_t m1_cols = self->n;

    size_t m2_rows = matrix->m;
    size_t m2_cols = matrix->n;

    double *dense_values = matrix->values;
    double *result_values = result->values;

    uint32_t row, row_start, row_len;

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        for (uint32_t j = 0; j < m2_cols; j++) {
            size_t result_index = row * m2_cols + j;
            double sum = result_values[result_index];
            for (uint32_t col = row_start; col < row_start + row_len; col++) {
                sum += data[col] * dense_values[m2_cols * indices[col] + j];
            }
            result_values[result_index] = sum;
        }
    })

    return 0;
}



int sparse_matrix_dot_sparse(sparse_matrix_t *self, sparse_matrix_t *other, double_matrix_t *result) {
    if (self->n != other->m || self->m != result->m || other->n != result->n) {
        return -1;
    }

    uint32_t *indptr = self->indptr->a;
    uint32_t *indices = self->indices->a;
    double *data = self->data->a;

    size_t m1_rows = self->m;
    size_t m1_cols = self->n;

    size_t m2_rows = other->m;
    size_t m2_cols = other->n;

    uint32_t *other_indptr = other->indptr->a;
    uint32_t *other_indices = other->indices->a;
    double *other_data = other->data->a;

    double *result_values = result->values;

    uint32_t row, row_start, row_len;

    sparse_matrix_foreach_row(self, row, row_start, row_len, {
        for (uint32_t i = row_start; i < row_start + row_len; i++) {
            uint32_t col = indices[i];
            if (col >= m2_rows) { return -1; }
            uint32_t m2_row_start = other_indptr[col];
            uint32_t m2_row_end = other_indptr[col + 1];
            double m1_data = data[i];

            for (uint32_t j = m2_row_start; j < m2_row_end; j++) {
                uint32_t m2_col = other_indices[j];
                size_t result_index = row * m2_cols + m2_col;
                double m2_data = other_data[j];

                result_values[result_index] += m1_data * m2_data;
            }

        }
    })

    return 0;
}




sparse_matrix_t *sparse_matrix_read(FILE *f) {
    sparse_matrix_t *sp = malloc(sizeof(sparse_matrix_t));
    if (sp == NULL) return NULL;

    sp->indptr = NULL;
    sp->indices = NULL;
    sp->data = NULL;

    if (!file_read_uint32(f, &sp->m) ||
        !file_read_uint32(f, &sp->n)) {
        goto exit_sparse_matrix_allocated;
    }

    uint64_t len_indptr;

    if (!file_read_uint64(f, &len_indptr)) {
        goto exit_sparse_matrix_allocated;
    }

    uint32_array *indptr = uint32_array_new_size((size_t)len_indptr);
    if (indptr == NULL) {
        goto exit_sparse_matrix_allocated;
    }

    if (!file_read_uint32_array(f, indptr->a, len_indptr)) {
        uint32_array_destroy(indptr);
        goto exit_sparse_matrix_allocated;
    }

    indptr->n = (size_t)len_indptr;
    sp->indptr = indptr;

    uint64_t len_indices;

    if (!file_read_uint64(f, &len_indices)) {
        goto exit_sparse_matrix_allocated;
    }

    uint32_array *indices = uint32_array_new_size(len_indices);
    if (indices == NULL) {
        goto exit_sparse_matrix_allocated;
    }

    if (!file_read_uint32_array(f, indices->a, len_indices)) {
        uint32_array_destroy(indices);
        goto exit_sparse_matrix_allocated;
    }

    indices->n = (size_t)len_indices;
    sp->indices = indices;

    uint64_t len_data;

    if (!file_read_uint64(f, &len_data)) {
        goto exit_sparse_matrix_allocated;
    }

    double_array *data = double_array_new_size(len_data);
    if (data == NULL) {
        goto exit_sparse_matrix_allocated;
    }

    if (!file_read_double_array(f, data->a, len_data)) {
        double_array_destroy(data);
        goto exit_sparse_matrix_allocated;
    }

    data->n = (size_t)len_data;
    sp->data = data;

    return sp;

exit_sparse_matrix_allocated:
    sparse_matrix_destroy(sp);
    return NULL;
}

bool sparse_matrix_write(sparse_matrix_t *self, FILE *f) {
    if (self == NULL || self->indptr == NULL || self->indices == NULL || self->data == NULL) {
        return false;
    }

    if (!file_write_uint32(f, self->m) ||
        !file_write_uint32(f, self->n)) {
        return false;
    }

    uint64_t len_indptr = self->indptr->n;

    if (!file_write_uint64(f, len_indptr)) {
        return false;
    }

    for (int i = 0; i < len_indptr; i++) {
        if (!file_write_uint32(f, self->indptr->a[i])) {
            return false;
        }
    }

    uint64_t len_indices = (uint64_t)self->indices->n;

    if (!file_write_uint64(f, len_indices)) {
        return false;
    }

    for (int i = 0; i < len_indices; i++) {
        if (!file_write_uint32(f, self->indices->a[i])) {
            return false;
        }
    }

    uint64_t len_data = (uint64_t)self->data->n;

    if (!file_write_uint64(f, len_data)) {
        return false;
    }

    for (int i = 0; i < len_data; i++) {
        if (!file_write_double(f, self->data->a[i])) {
            return false;
        }
    }

    return true;
}
