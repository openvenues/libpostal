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

bool stochastic_gradient_descent(matrix_t *theta, matrix_t *gradient, double alpha);
bool stochastic_gradient_descent_scheduled(matrix_t *theta, matrix_t *gradient, float lambda, uint32_t t, double gamma_0);

#endif