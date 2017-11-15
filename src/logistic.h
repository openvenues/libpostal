#ifndef LOGISTIC_H
#define LOGISTIC_H

#include <stdlib.h>
#include <stdint.h>

#include "matrix.h"

double sigmoid(double x);
void sigmoid_vector(double *x, size_t n);
void softmax_vector(double *x, size_t n);
void softmax_matrix(double_matrix_t *matrix);

#endif