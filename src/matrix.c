#include "matrix.h"

matrix_t *matrix_new(size_t m, size_t n) {
    matrix_t *matrix = malloc(sizeof(matrix_t));

    if (matrix == NULL) {
        return NULL;
    }

    matrix->m = m;
    matrix->n = n;

    matrix->values = malloc(sizeof(double) * m * n);
    if (matrix->values == NULL) {
        free(matrix);
        return NULL;
    }

    return matrix;

}

matrix_t *matrix_copy(matrix_t *self) {
    matrix_t *cpy = matrix_new(self->m, self->n);
    size_t num_values = self->m * self->n;
    memcpy(cpy->values, self->values, num_values * sizeof(double));
    return cpy;
}

inline void matrix_init_values(matrix_t *self, double *values) {
    size_t num_values = self->m * self->n;
    memcpy(self->values, values, num_values * sizeof(double));
}

inline void matrix_zero(matrix_t *self) {
    memset(self->values, 0, self->m * self->n * sizeof(double));
}

inline void matrix_set(matrix_t *self, double value) {
    double_array_set(self->values, self->m * self->n, value);
}

inline void matrix_set_row(matrix_t *self, size_t index, double *row) {
    size_t offset = index * self->m;
    double *values = self->values;
    size_t n = self->n;
    memcpy(values + offset, row, n * sizeof(double));
}

inline void matrix_set_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->m + col_index;
    double *values = self->values;
    values[offset] = value;
}

inline double matrix_get(matrix_t *self, size_t row_index, size_t col_index) {
    size_t index = row_index * self->n + col_index;
    return self->values->a[index];
}

inline matrix_t *matrix_new_value(size_t m, size_t n, double value) {
    matrix_t *matrix = matrix_new(m, n);
    matrix_set(matrix, value);
    return matrix;
}

inline matrix_t *matrix_new_zeros(size_t m, size_t n) {
    return matrix_new_value(m, n, 0.0);
}

inline matrix_t *matrix_new_ones(size_t m, size_t n) {
    return matrix_new_value(m, n, 1.0);
}

matrix_t *matrix_new_values(size_t m, size_t n, double *values) {
    matrix_t *matrix = matrix_new(m, n);
    memcpy(matrix->values, values, m * n * sizeof(double));
    return matrix;
}


inline void matrix_div(matrix_t *self, double value) {
    double_array_div(self->values, value, self->m * self->n);
}


inline void matrix_mul(matrix_t *self, double value) {
    double_array_mul(self->values, value, self->m * self->n);
}


inline void matrix_add(matrix_t *self, double value) {
    double_array_add(self->values, self->m * self->n, value);
}


inline void matrix_sub(matrix_t *self, double value) {
    double_array_sub(self->values, value, self->m * self->n);
}


void matrix_dot_vector(matrix_t *self, double *vec, double *result) {
    double *values = self->values;
    size_t n = self->n;
    for (int i = 0; i < self->m; i++) {
        for (int j = 0; j < n; j++) {
            result[i] += values[n * i + j] * vec[j];
        }
    }
}

int matrix_dot_matrix(matrix_t *m1, matrix_t *m2, matrix_t *result) {
    if (m1->n != m2->m || m1->m != result->m || m2->n != result->n) {
        return -1;
    }

    size_t m1_rows = m1->m;
    size_t m1_cols = m1->n;
    size_t m2_rows = m2->m;
    size_t m2_cols = m2->n;

    double *m1_values = m1->values;
    double *m2_values = m2->values;
    double *result_values = result->values;

    for (int i = 0; i < m1_rows; i++) {
        for (int j = 0; j < m2_cols; j++) {
            size_t result_index = m2_cols * i + j;
            result_values[result_index] = 0.0;
            for (int k = 0; k < m2_rows; k++) {
                result_values[result_index] += m1_values[m1_cols * i + k] * m2_values[m2_cols * k + j];
            }
        }
    }

    return 0;
}

matrix_t *matrix_read(FILE *f) {
    matrix_t *mat = malloc(sizeof(matrix_t));
    if (mat == NULL) return NULL;

    mat->data = NULL;

    if (!file_read_uint32(f, &mat->m) ||
        !file_read_uint32(f, &mat->n)) {
        goto exit_sparse_matrix_allocated;
    }

    size_t len_data = (size_t)mat->m * (size_t)mat->n;

    double_array *data = double_array_new_size(len_data);
    if (data == NULL) {
        printf("data alloc\n");
        goto exit_matrix_allocated;
    }

    for (size_t i = 0; i < len_data; i++) {
        if (!file_read_double(f, data->a + i)) {
            printf("data\n");
            goto exit_sparse_matrix_allocated;
        }
    }

    data->n = (size_t)len_data;
    mat->data = data;

    return mat;

exit_matrix_allocated:
    matrix_destroy(mat);
    return NULL;
}

bool matrix_write(matrix_t *self, FILE *f) {
    if (self == NULL || self->data == NULL) {
        return false;
    }

    if (!file_write_uint32(f, self->m) ||
        !file_write_uint32(f, self->n)) {
        return false;
    }

    uint64_t len_data = (uint64_t)self->m * (uint64_t)self->n;

    for (int i = 0; i < len_data; i++) {
        if (!file_write_double(f, self->data->a[i])) {
            return false;
        }
    }

    return true;
}


void matrix_destroy(matrix_t *self) {
    if (self == NULL) return;

    if (self->values != NULL) {
        free(self->values);
    }

    free(self);
}
