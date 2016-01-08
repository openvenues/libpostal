
#ifndef LOGISTIC_REGRESSION_TRAINER_H
#define LOGISTIC_REGRESSION_TRAINER_H

#include <stdio.h>
#include <stdlib.h>

#include "averaged_perceptron_tagger.h"
#include "collections.h"
#include "features.h"
#include "logistic_regression.h"
#include "minibatch.h"
#include "sparse_matrix.h"
#include "string_utils.h"
#include "stochastic_gradient_descent.h"
#include "tokens.h"
#include "trie.h"

#define DEFAULT_GAMMA_SCHEDULE {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0}
#define DEFAUlT_LAMBDA_SCHEDULE {0.0, 1e-5, 1e-4, 0.001, 0.01, 0.1, \
                                 0.2, 0.5, 1.0, 2.0, 5.0, 10.0}

#define DEFAULT_GAMMA_0 1.0
#define DEFAULT_LAMBDA 0.0
#define DEFAULT_GAMMA 0.1

/**
 * Helper struct for training logistic regression model
 */

typedef struct logistic_regression_trainer {
    trie_t *feature_ids;                // Trie mapping features to array indices
    size_t num_features;                // Number of features
    khash_t(str_uint32) *label_ids;     // Hashtable mapping labels to array indices
    size_t num_labels;                  // Number of labels
    matrix_t *weights;                  // Matrix of logistic regression weights
    uint32_array *last_updated;         // Array of length N indicating the last time each feature was updated
    double lambda;                      // Regularization weight
    uint32_t iters;                     // Number of iterations, used to decay learning rate
    uint32_t epochs;                    // Number of epochs
    double gamma_0;                     // Initial learning rate
    double gamma;                       // Simple scalar learning rate
} logistic_regression_trainer_t;


logistic_regression_trainer_t *logistic_regression_trainer_init(trie_t *feature_ids, khash_t(str_uint32) *label_ids);

bool logistic_regression_trainer_train_batch(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels);
double logistic_regression_trainer_batch_cost(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels);

void logistic_regression_trainer_destroy(logistic_regression_trainer_t *self);

#endif
