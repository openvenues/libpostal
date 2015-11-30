#ifndef MATRIX_H
#define MATRIX_H

#include <stdlib.h>
#include <stdint.h>

#include "collections.h"

typedef struct matrix {
    size_t m;
    size_t n;
    double *values;
} matrix_t;

matrix_t *matrix_new(size_t m, size_t n);
matrix_t *matrix_new_value(size_t m, size_t n, double value);
matrix_t *matrix_new_values(size_t m, size_t n, double *values);
matrix_t *matrix_new_zeros(size_t m, size_t n);
matrix_t *matrix_new_ones(size_t m, size_t n);

matrix_t *matrix_copy(matrix_t *self);

void matrix_init_values(matrix_t *self, double *values);
void matrix_set(matrix_t *self, double value);
void matrix_zero(matrix_t *self);
void matrix_set_row(matrix_t *self, size_t index, double *row);
void matrix_set_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);

double matrix_get(matrix_t *self, size_t row_index, size_t col_index);

void matrix_add(matrix_t *self, double value);
void matrix_sub(matrix_t *self, double value);
void matrix_mul(matrix_t *self, double value);
void matrix_div(matrix_t *self, double value);
void matrix_dot_vector(matrix_t *self, double *vec, double *result);
int matrix_dot_matrix(matrix_t *m1, matrix_t *m2, matrix_t *result);

matrix_t *matrix_read(FILE *f);
bool matrix_write(matrix_t *self, FILE *f);

void matrix_destroy(matrix_t *self);

#endif