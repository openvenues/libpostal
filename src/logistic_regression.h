/*
logistic_regression.h
---------------------

Cost function and gradient for multinomial logistic regression model.

Multinomial logistic regression is a generalization of regular logistic 
regression in which we predict a probability distribution over classes using:

exp(x_i) / sum(exp(x))

This is sometimes referred to as the softmax function and thus the model
may be called softmax regression.

*/

#ifndef LOGISTIC_REGRESSION_MODEL_H
#define LOGISTIC_REGRESSION_MODEL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "collections.h"
#include "matrix.h"
#include "sparse_matrix.h"

bool logistic_regression_model_expectation(matrix_t *theta, sparse_matrix_t *x, matrix_t *p_y);
double logistic_regression_cost_function(matrix_t *theta, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, double lambda);
bool logistic_regression_gradient(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, double lambda);
bool logistic_regression_gradient_sparse(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, 
                                         uint32_array *x_cols, double lambda);

#endif
