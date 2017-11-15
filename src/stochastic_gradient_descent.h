/*
Stochastic gradient descent implementation

Based on Bob Carpenter's Lazy Sparse Stochastic Gradient Descent for Regularized Mutlinomial Logistic Regression:
https://lingpipe.files.wordpress.com/2008/04/lazysgdregression.pdf

Learning rate update based on Leon Bottou's Stochastic Gradient Descent Tricks:
http://leon.bottou.org/publications/pdf/tricks-2012.pdf

Learning rate calculated as:
gamma_t = gamma_0(1 + gamma_0 * lambda * t)^-1
*/
#ifndef STOCHASTIC_GRADIENT_DESCENT_H
#define STOCHASTIC_GRADIENT_DESCENT_H

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "matrix.h"
#include "regularization.h"
#include "sparse_matrix.h"

typedef struct sgd_trainer {
    double_matrix_t *theta;
    regularization_type_t reg_type;
    double lambda;
    double gamma_0;
    bool fit_intercept;
    uint32_t iterations;
    uint32_array *last_updated;
    double_array *penalties;
} sgd_trainer_t;

sgd_trainer_t *sgd_trainer_new(size_t m, size_t n, bool fit_intercept, regularization_type_t reg_type, double lambda, double gamma_0);

bool sgd_trainer_reset_params(sgd_trainer_t *self, double lambda, double gamma_0);
bool stochastic_gradient_descent_update(sgd_trainer_t *self, double_matrix_t *gradient, size_t batch_size);
bool stochastic_gradient_descent_update_sparse(sgd_trainer_t *self, double_matrix_t *gradient, uint32_array *update_indices, size_t batch_size);
double stochastic_gradient_descent_reg_cost(sgd_trainer_t *self, uint32_array *indices, size_t batch_size);
bool stochastic_gradient_descent_set_regularized_weights(sgd_trainer_t *self, double_matrix_t *w, uint32_array *indices);
bool stochastic_gradient_descent_regularize_weights(sgd_trainer_t *self);
double_matrix_t *stochastic_gradient_descent_get_weights(sgd_trainer_t *self);
sparse_matrix_t *stochastic_gradient_descent_get_weights_sparse(sgd_trainer_t *self);

void sgd_trainer_destroy(sgd_trainer_t *self);

#endif