#include "matrix.h"
#include "file_utils.h"

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

bool matrix_resize(matrix_t *self, size_t m, size_t n) {
    if (self == NULL) return false;

    // If the new size is smaller, don't reallocate, just ignore the extra values
    if (m * n > (self->m * self->n)) {
        self->values = realloc(self->values, sizeof(double) * m * n);
        if (self->values == NULL) {
            return false;
        }        
    }

    self->m = m;
    self->n = n;

    // Set all values to 0
    matrix_zero(self);

    return true;
}

matrix_t *matrix_new_copy(matrix_t *self) {
    matrix_t *cpy = matrix_new(self->m, self->n);
    size_t num_values = self->m * self->n;
    memcpy(cpy->values, self->values, num_values);

    return cpy;
}

bool matrix_copy(matrix_t *self, matrix_t *other) {
    if (self->m != other->m || self->n != other->n) {
        return false;
    }
    size_t num_values = self->n * self->n;

    memcpy(other->values, self->values, num_values * sizeof(double));
    return true;
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
    size_t offset = index * self->n;
    double *values = self->values;
    size_t n = self->n;
    memcpy(values + offset, row, n * sizeof(double));
}

inline void matrix_set_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->n + col_index;
    self->values[offset] = value;
}

inline void matrix_add_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->n + col_index;
    self->values[offset] += value;
}

inline void matrix_sub_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->n + col_index;
    self->values[offset] -= value;
}

inline void matrix_mul_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->n + col_index;
    self->values[offset] *= value;
}

inline void matrix_div_scalar(matrix_t *self, size_t row_index, size_t col_index, double value) {
    size_t offset = row_index * self->n + col_index;
    self->values[offset] /= value;
}

inline double matrix_get(matrix_t *self, size_t row_index, size_t col_index) {
    size_t index = row_index * self->n + col_index;
    return self->values[index];
}

inline double *matrix_get_row(matrix_t *self, size_t row_index) {
    size_t index = row_index * self->n;
    return self->values + index;
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

inline bool matrix_div_matrix(matrix_t *self, matrix_t *other) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_div_array(self->values, other->values, self->m * self->n);
    return true;
}

inline bool matrix_div_matrix_times_scalar(matrix_t *self, matrix_t *other, double v) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_div_array_times_scalar(self->values, other->values, v, self->m * self->n);
    return true;
}


inline void matrix_mul(matrix_t *self, double value) {
    double_array_mul(self->values, value, self->m * self->n);
}

inline bool matrix_mul_matrix(matrix_t *self, matrix_t *other) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_mul_array(self->values, other->values, self->m * self->n);
    return true;
}

inline bool matrix_mul_matrix_times_scalar(matrix_t *self, matrix_t *other, double v) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_mul_array_times_scalar(self->values, other->values, v, self->m * self->n);
    return true;
}

inline void matrix_add(matrix_t *self, double value) {
    double_array_add(self->values, self->m * self->n, value);
}


inline bool matrix_add_matrix(matrix_t *self, matrix_t *other) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_add_array(self->values, other->values, self->m * self->n);
    return true;
}

inline bool matrix_add_matrix_times_scalar(matrix_t *self, matrix_t *other, double v) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_add_array_times_scalar(self->values, other->values, v, self->m * self->n);
    return true;
}

inline void matrix_sub(matrix_t *self, double value) {
    double_array_sub(self->values, value, self->m * self->n);
}

inline bool matrix_sub_matrix(matrix_t *self, matrix_t *other) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_sub_array(self->values, other->values, self->m * self->n);
    return true;
}

inline bool matrix_sub_matrix_times_scalar(matrix_t *self, matrix_t *other, double v) {
    if (self->m != other->m || self->n != other->n) return false;
    double_array_sub_array_times_scalar(self->values, other->values, v, self->m * self->n);
    return true;
}


inline void matrix_log(matrix_t *self) {
    double_array_log(self->values, self->m * self->n);
}

inline void matrix_exp(matrix_t *self) {
    double_array_exp(self->values, self->m * self->n);
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

inline bool matrix_dot_matrix(matrix_t *m1, matrix_t *m2, matrix_t *result) {
    if (m1->n != m2->m || m1->m != result->m || m2->n != result->n) {
        return false;
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

    return true;
}

matrix_t *matrix_read(FILE *f) {
    matrix_t *mat = malloc(sizeof(matrix_t));
    if (mat == NULL) return NULL;

    mat->values = NULL;

    uint64_t m = 0;
    uint64_t n = 0;

    if (!file_read_uint64(f, &m) ||
        !file_read_uint64(f, &n)) {
        goto exit_matrix_allocated;
    }

    mat->m = (size_t)m;
    mat->n = (size_t)n;

    size_t len_data = mat->m * mat->n;

    double *data = malloc(len_data * sizeof(double));
    if (data == NULL) {
        printf("data alloc\n");
        goto exit_matrix_allocated;
    }

    if (!file_read_double_array(f, data, len_data)) {
        free(data);
        goto exit_matrix_allocated;
    }

    mat->values = data;

    return mat;

exit_matrix_allocated:
    matrix_destroy(mat);
    return NULL;
}

bool matrix_write(matrix_t *self, FILE *f) {
    if (self == NULL || self->values == NULL) {
        return false;
    }

    if (!file_write_uint64(f, (uint64_t)self->m) ||
        !file_write_uint64(f, (uint64_t)self->n)) {
        return false;
    }

    uint64_t len_data = (uint64_t)self->m * (uint64_t)self->n;

    for (uint64_t i = 0; i < len_data; i++) {
        if (!file_write_double(f, self->values[i])) {
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
