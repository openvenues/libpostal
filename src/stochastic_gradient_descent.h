/*
Stochastic gradient descent implementation

Based on Leon Bottou's Stochastic Gradient Descent Tricks:
http://leon.bottou.org/publications/pdf/tricks-2012.pdf

Learning rate calculated as:
gamma_t = gamma_0(1 + gamma_0 * lambda * t)^-1

*/
#ifndef STOCHASTIC_GRADIENT_DESCENT_H
#define STOCHASTIC_GRADIENT_DESCENT_H

#include <stdlib.h>
#include <stdbool.h>

#include "matrix.h"

bool stochastic_gradient_descent(matrix_t *theta, matrix_t *gradient, double gamma);
bool stochastic_gradient_descent_sparse(matrix_t *theta, matrix_t *gradient, uint32_array *update_indices, double gamma);
bool stochastic_gradient_descent_regularize_weights(matrix_t *theta, uint32_array *update_indices, uint32_array *last_updated, uint32_t t, double lambda, double gamma_0);
bool stochastic_gradient_descent_finalize_weights(matrix_t *theta, uint32_array *last_updated, uint32_t t, double lambda, double gamma_0);
double stochastic_gradient_descent_gamma_t(double gamma_0, double lambda, uint32_t t);


#endif