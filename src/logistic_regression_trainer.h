
#ifndef LOGISTIC_REGRESSION_TRAINER_H
#define LOGISTIC_REGRESSION_TRAINER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "averaged_perceptron_tagger.h"
#include "collections.h"
#include "features.h"
#include "ftrl.h"
#include "logistic_regression.h"
#include "minibatch.h"
#include "sparse_matrix.h"
#include "string_utils.h"
#include "stochastic_gradient_descent.h"
#include "tokens.h"
#include "trie.h"

/**
 * Helper struct for training logistic regression model
 */

 typedef enum {
    LOGISTIC_REGRESSION_OPTIMIZER_SGD,
    LOGISTIC_REGRESSION_OPTIMIZER_FTRL
 } logistic_regression_optimizer_type;

typedef struct logistic_regression_trainer {
    trie_t *feature_ids;                                // Trie mapping features to array indices
    size_t num_features;                                // Number of features
    khash_t(str_uint32) *label_ids;                     // Hashtable mapping labels to array indices
    size_t num_labels;                                  // Number of labels
    double_matrix_t *gradient;                          // Gradient matrix to be reused
    khash_t(int_uint32) *unique_columns;                // Unique columns set
    uint32_array *batch_columns;                        // Unique columns as array
    double_matrix_t *batch_weights;                     // Weights updated in this batch
    uint32_t epochs;                                    // Number of epochs
    logistic_regression_optimizer_type optimizer_type;  // Trainer type
    union {
        sgd_trainer_t *sgd;                             // Stochastic (ok, minibatch) gradient descent
        ftrl_trainer_t *ftrl;                           // Follow-the-regularized-leader (FTRL) Proximal
    } optimizer;
} logistic_regression_trainer_t;

logistic_regression_trainer_t *logistic_regression_trainer_init_sgd(trie_t *feature_ids, khash_t(str_uint32) *label_ids, bool fit_intercept, regularization_type_t reg_type, double lambda, double gamma_0);
logistic_regression_trainer_t *logistic_regression_trainer_init_ftrl(trie_t *feature_ids, khash_t(str_uint32) *label_ids, double lambda1, double lambda2, double alpha, double beta);
bool logistic_regression_trainer_reset_params_sgd(logistic_regression_trainer_t *self, double lambda, double gamma_0);
bool logistic_regression_trainer_reset_params_ftrl(logistic_regression_trainer_t *self, double alpha, double beta, double lambda1, double lambda2);
bool logistic_regression_trainer_train_minibatch(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels);
double logistic_regression_trainer_minibatch_cost(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels);
double logistic_regression_trainer_minibatch_cost_regularized(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels);
double logistic_regression_trainer_regularization_cost(logistic_regression_trainer_t *self, size_t m);

double_matrix_t *logistic_regression_trainer_get_weights(logistic_regression_trainer_t *self);
double_matrix_t *logistic_regression_trainer_get_regularized_weights(logistic_regression_trainer_t *self);
double_matrix_t *logistic_regression_trainer_final_weights(logistic_regression_trainer_t *self);
sparse_matrix_t *logistic_regression_trainer_final_weights_sparse(logistic_regression_trainer_t *self);

void logistic_regression_trainer_destroy(logistic_regression_trainer_t *self);

#endif
