/*
Follow-the-regularized-leader (FTRL) Proximal inference

This is a regularization scheme for online gradient descent
which produces sparse solutions.

From: https://research.google.com/pubs/archive/41159.pdf
*/

#ifndef FTRL_H
#define FTRL_H

#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "float_utils.h"
#include "matrix.h"
#include "sparse_matrix.h"

typedef struct ftrl_trainer {
    // Dense weights matrix, only needed to store at training time
    double_matrix_t *z;
    // Whether to consider feature_id=0 the intercept
    bool fit_intercept;
    // number of features (may change)
    size_t num_features;
    // alpha hyperparameter
    double alpha;
    // beta hyperparameter
    double beta;
    // Coefficient for L1 regularization
    double lambda1;
    // Coefficient for L2 regularization
    double lambda2;
    // Vector of learning rates
    double_array *learning_rates;
} ftrl_trainer_t;

ftrl_trainer_t *ftrl_trainer_new(size_t m, size_t n, bool fit_intercept, double alpha, double beta, double lambda1, double lambda2);

bool ftrl_trainer_reset_params(ftrl_trainer_t *self, double alpha, double beta, double lambda1, double lambda2);

bool ftrl_trainer_extend(ftrl_trainer_t *trainer, size_t m);

bool ftrl_set_weights(ftrl_trainer_t *trainer, double_matrix_t *w, uint32_array *indices);

double ftrl_reg_cost(ftrl_trainer_t *self, double_matrix_t *theta, uint32_array *indices, size_t batch_size);

bool ftrl_update_gradient(ftrl_trainer_t *self, double_matrix_t *gradient, double_matrix_t *weights, uint32_array *indices, size_t batch_size);

double_matrix_t *ftrl_weights_finalize(ftrl_trainer_t *trainer);
sparse_matrix_t *ftrl_weights_finalize_sparse(ftrl_trainer_t *trainer);

void ftrl_trainer_destroy(ftrl_trainer_t *self);

#endif