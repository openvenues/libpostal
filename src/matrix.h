#ifndef MATRIX_H
#define MATRIX_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

bool matrix_resize(matrix_t *self, size_t m, size_t n);

matrix_t *matrix_new_copy(matrix_t *self);
bool matrix_copy(matrix_t *self, matrix_t *other);

void matrix_init_values(matrix_t *self, double *values);
void matrix_set(matrix_t *self, double value);
void matrix_zero(matrix_t *self);
void matrix_set_row(matrix_t *self, size_t index, double *row);
void matrix_set_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);
void matrix_add_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);
void matrix_sub_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);
void matrix_mul_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);
void matrix_div_scalar(matrix_t *self, size_t row_index, size_t col_index, double value);

double matrix_get(matrix_t *self, size_t row_index, size_t col_index);
double *matrix_get_row(matrix_t *self, size_t row_index);

void matrix_add(matrix_t *self, double value);
bool matrix_add_matrix(matrix_t *self, matrix_t *other);
bool matrix_add_matrix_times_scalar(matrix_t *self, matrix_t *other, double v);
void matrix_sub(matrix_t *self, double value);
bool matrix_sub_matrix(matrix_t *self, matrix_t *other);
bool matrix_sub_matrix_times_scalar(matrix_t *self, matrix_t *other, double v);
void matrix_mul(matrix_t *self, double value);
bool matrix_mul_matrix(matrix_t *self, matrix_t *other);
bool matrix_mul_matrix_times_scalar(matrix_t *self, matrix_t *other, double v);
void matrix_div(matrix_t *self, double value);
bool matrix_div_matrix(matrix_t *self, matrix_t *other);
bool matrix_div_matrix_times_scalar(matrix_t *self, matrix_t *other, double v);

void matrix_log(matrix_t *self);
void matrix_exp(matrix_t *self);

void matrix_dot_vector(matrix_t *self, double *vec, double *result);
bool matrix_dot_matrix(matrix_t *m1, matrix_t *m2, matrix_t *result);

matrix_t *matrix_read(FILE *f);
bool matrix_write(matrix_t *self, FILE *f);

void matrix_destroy(matrix_t *self);

#endif
