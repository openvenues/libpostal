#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

#include "log/log.h"
#include "address_dictionary.h"
#include "cartesian_product.h"
#include "collections.h"
#include "language_classifier.h"
#include "language_classifier_io.h"
#include "logistic_regression.h"
#include "logistic_regression_trainer.h"
#include "shuffle.h"
#include "sparse_matrix.h"
#include "sparse_matrix_utils.h"
#include "stochastic_gradient_descent.h"
#include "transliterate.h"

#define LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD 3.0
#define LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD 100

#define LOG_BATCH_INTERVAL 10
#define COMPUTE_COST_INTERVAL 100
#define COMPUTE_CV_INTERVAL 1000

#define LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES 50

// Hyperparameters for stochastic gradient descent

static double GAMMA_SCHEDULE[] = {0.01, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
static const size_t GAMMA_SCHEDULE_SIZE = sizeof(GAMMA_SCHEDULE) / sizeof(double);

#define DEFAULT_GAMMA_0 10.0

#define REGULARIZATION_SCHEDULE {0.0, 1e-7, 1e-6, 1e-5, 1e-4, 0.001, 0.01, 0.1, \
                                 0.2, 0.5, 1.0, 2.0, 5.0, 10.0}

static double L2_SCHEDULE[] = REGULARIZATION_SCHEDULE;
static const size_t L2_SCHEDULE_SIZE  = sizeof(L2_SCHEDULE) / sizeof(double);

static double L1_SCHEDULE[] = REGULARIZATION_SCHEDULE;
static const size_t L1_SCHEDULE_SIZE  = sizeof(L1_SCHEDULE) / sizeof(double);

#define DEFAULT_L2 1e-6
#define DEFAULT_L1 1e-4

// Hyperparameters for FTRL-Proximal

static double ALPHA_SCHEDULE[] = {0.01, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
static const size_t ALPHA_SCHEDULE_SIZE = sizeof(ALPHA_SCHEDULE) / sizeof(double);
static double DEFAULT_BETA = 1.0;

#define DEFAULT_ALPHA 10.0

#define TRAIN_EPOCHS 10

#define HYPERPARAMETER_EPOCHS 5

logistic_regression_trainer_t *language_classifier_init_params(char *filename, double feature_count_threshold, uint32_t label_count_threshold, size_t minibatch_size, logistic_regression_optimizer_type optim_type, regularization_type_t reg_type) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return NULL;
    }

    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;
    khash_t(str_double) *feature_counts = kh_init(str_double);
    khash_t(str_uint32) *label_counts = kh_init(str_uint32);

    size_t num_batches = 0;

    // Count features and labels
    while ((minibatch = language_classifier_data_set_get_minibatch_with_size(data_set, NULL, minibatch_size)) != NULL) {
        if (!count_labels_minibatch(label_counts, minibatch->labels)) {
            log_error("Counting minibatch labeles failed\n");
            exit(EXIT_FAILURE);
        }

        if (num_batches % LOG_BATCH_INTERVAL == 0 && num_batches > 0) {
            log_info("Counting labels, did %zu examples\n", num_batches * minibatch_size);
        }

        num_batches++;

        language_classifier_minibatch_destroy(minibatch);
    }
    log_info("Done counting labels\n");

    language_classifier_data_set_destroy(data_set);

    data_set = language_classifier_data_set_init(filename);
    num_batches = 0;

    khash_t(str_uint32) *label_ids = select_labels_threshold(label_counts, label_count_threshold);
    if (label_ids == NULL) {
        log_error("Error creating labels\n");
        exit(EXIT_FAILURE);
    }

    size_t num_labels = kh_size(label_ids);
    log_info("num_labels=%zu\n", num_labels);

    // Don't free the label strings as the pointers are reused in select_labels_threshold
    kh_destroy(str_uint32, label_counts);

    // Run through the training set again, counting only features which co-occur with valid classes
    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, label_ids)) != NULL) {
        if (!count_features_minibatch(feature_counts, minibatch->features, true)){
            log_error("Counting minibatch features failed\n");
            exit(EXIT_FAILURE);
        }

        if (num_batches % LOG_BATCH_INTERVAL == 0 && num_batches > 0) {
            log_info("Counting features, did %zu examples\n", num_batches * minibatch_size);
        }

        num_batches++;

        language_classifier_minibatch_destroy(minibatch);
    }

    log_info("Done counting features, finalizing\n");

    language_classifier_data_set_destroy(data_set);

    // Discard rare features using a count threshold (can be 1) and convert them to trie
    trie_t *feature_ids = select_features_threshold(feature_counts, feature_count_threshold);
    if (feature_ids == NULL) {
        log_error("Error creating features trie\n");
        exit(EXIT_FAILURE);
    }

    // Need to free the keys here as trie uses its own memory
    const char *key;
    kh_foreach_key(feature_counts, key, {
        free((char *)key);
    })
    kh_destroy(str_double, feature_counts);


    logistic_regression_trainer_t *trainer = NULL;

    if (optim_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        bool fit_intercept = true;
        double default_lambda = 0.0;
        if (reg_type == REGULARIZATION_L2){
            default_lambda = DEFAULT_L2;
        } else if (reg_type == REGULARIZATION_L1) {
            default_lambda = DEFAULT_L1;
        }
        trainer = logistic_regression_trainer_init_sgd(feature_ids, label_ids, fit_intercept, reg_type, default_lambda, DEFAULT_GAMMA_0);
    } else if (optim_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        trainer = logistic_regression_trainer_init_ftrl(feature_ids, label_ids, DEFAULT_ALPHA, DEFAULT_BETA, DEFAULT_L1, DEFAULT_L2);
    }

    return trainer;
}

logistic_regression_trainer_t *language_classifier_init_optim_reg(char *filename, size_t minibatch_size, logistic_regression_optimizer_type optim_type, regularization_type_t reg_type) {
    return language_classifier_init_params(filename, LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD, LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD, minibatch_size, optim_type, reg_type);
}

logistic_regression_trainer_t *language_classifier_init_sgd_reg(char *filename, size_t minibatch_size, regularization_type_t reg_type) {
    return language_classifier_init_params(filename, LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD, LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD, minibatch_size, LOGISTIC_REGRESSION_OPTIMIZER_SGD, reg_type);
}

logistic_regression_trainer_t *language_classifier_init_ftrl(char *filename, size_t minibatch_size) {
    return language_classifier_init_params(filename, LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD, LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD, minibatch_size, LOGISTIC_REGRESSION_OPTIMIZER_FTRL, REGULARIZATION_NONE);
}

double compute_cv_accuracy(logistic_regression_trainer_t *trainer, char *filename) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    uint32_t correct = 0;
    uint32_t total = 0;

    double_matrix_t *p_y = double_matrix_new_zeros(LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE, trainer->num_labels);

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, trainer->label_ids)) != NULL) {
        sparse_matrix_t *x = feature_matrix(trainer->feature_ids, minibatch->features);
        uint32_array *y = label_vector(trainer->label_ids, minibatch->labels);

        if (!double_matrix_resize_aligned(p_y, x->m, trainer->num_labels, 16)) {
            log_error("resize p_y failed\n");
            exit(EXIT_FAILURE);
        }
        double_matrix_zero(p_y);

        if (!sparse_matrix_add_unique_columns_alias(x, trainer->unique_columns, trainer->batch_columns)) {
            log_error("Error adding unique columns\n");
            exit(EXIT_FAILURE);
        }

        double_matrix_t *theta = logistic_regression_trainer_get_weights(trainer);

        if (!logistic_regression_model_expectation(theta, x, p_y)) {
            log_error("Predict cv batch failed\n");
            exit(EXIT_FAILURE);
        }

        double *row;
        for (size_t i = 0; i < p_y->m; i++) {
            row = double_matrix_get_row(p_y, i);

            int64_t predicted = double_array_argmax(row, p_y->n);
            if (predicted < 0) {
                log_error("Error in argmax\n");
                exit(EXIT_FAILURE);
            }
            uint32_t y_i = y->a[i];
            if (y_i == (uint32_t)predicted) {
                correct++;
            }

            total++;
        }

        sparse_matrix_destroy(x);
        uint32_array_destroy(y);

        language_classifier_minibatch_destroy(minibatch);
    }

    language_classifier_data_set_destroy(data_set);
    double_matrix_destroy(p_y);

    double accuracy = (double)correct / total;
    return accuracy;
}



double compute_total_cost(logistic_regression_trainer_t *trainer, char *filename, ssize_t compute_batches) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    double total_cost = 0.0;
    size_t num_batches = 0;
    size_t num_examples = 0;

    // Need to regularize the weights
    double_matrix_t *theta = logistic_regression_trainer_get_regularized_weights(trainer);

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, trainer->label_ids)) != NULL) {

        double batch_cost = logistic_regression_trainer_minibatch_cost(trainer, minibatch->features, minibatch->labels);
        total_cost += batch_cost;

        num_examples += minibatch->features->n;

        language_classifier_minibatch_destroy(minibatch);

        num_batches++;

        if (compute_batches > 0 && num_batches == (size_t)compute_batches) {
            break;
        }
    }

    double reg_cost = logistic_regression_trainer_regularization_cost(trainer, num_examples);
    log_info("cost = %f, reg_cost = %f, m = %zu\n", total_cost, reg_cost, num_examples);
    total_cost += reg_cost;

    language_classifier_data_set_destroy(data_set);

    return total_cost;
}


bool language_classifier_train_epoch(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, ssize_t train_batches, size_t minibatch_size) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return false;
    }

    #if defined(HAVE_SHUF) || defined(HAVE_GSHUF)
    log_info("Shuffling\n");

    if (!shuffle_file_chunked_size(filename, DEFAULT_SHUFFLE_CHUNK_SIZE)) {
        log_error("Error in shuffle\n");
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    log_info("Shuffle complete\n");
    #endif

    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    size_t num_batches = 0;
    double batch_cost = 0.0;
    double total_cost = 0.0;
    double last_cost = 0.0;

    double train_cost = 0.0;
    double cv_accuracy = 0.0;

    while ((minibatch = language_classifier_data_set_get_minibatch_with_size(data_set, trainer->label_ids, minibatch_size)) != NULL) {
        bool compute_cost = num_batches % COMPUTE_COST_INTERVAL == 0;
        bool compute_cv = num_batches % COMPUTE_CV_INTERVAL == 0 && num_batches > 0 && cv_filename != NULL;

        if (num_batches % LOG_BATCH_INTERVAL == 0 && num_batches > 0) {
            log_info("Epoch %u, doing %zu examples\n", trainer->epochs, num_batches * minibatch_size);
        }

        if (compute_cost) {
            train_cost = logistic_regression_trainer_minibatch_cost_regularized(trainer, minibatch->features, minibatch->labels);
            log_info("cost = %f\n", train_cost);
        }

        if (!logistic_regression_trainer_train_minibatch(trainer, minibatch->features, minibatch->labels)){
            log_error("Train batch failed\n");
            exit(EXIT_FAILURE);
        }

        if (compute_cv) {
            cv_accuracy = compute_cv_accuracy(trainer, cv_filename);
            log_info("cv accuracy=%f\n", cv_accuracy);
        }

        num_batches++;

        if (train_batches > 0 && num_batches == (size_t)train_batches) {
            log_info("Epoch %u, trained %zu examples\n", trainer->epochs, num_batches * minibatch_size);
            train_cost = logistic_regression_trainer_minibatch_cost_regularized(trainer, minibatch->features, minibatch->labels);
            log_info("cost = %f\n", train_cost);
            break;
        }

        language_classifier_minibatch_destroy(minibatch);

    }

    language_classifier_data_set_destroy(data_set);

    return true;
}

static double language_classifier_cv_cost(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, size_t minibatch_size, bool *diverged) {
    ssize_t cost_batches;
    char *cost_file;

    if (cv_filename == NULL) {
        cost_file = filename;
        cost_batches = LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES;
    } else {
        cost_file = cv_filename;
        cost_batches = -1;
    }

    double initial_cost = compute_total_cost(trainer, cost_file, cost_batches);

    for (size_t k = 0; k < HYPERPARAMETER_EPOCHS; k++) {
        trainer->epochs = k;

        if (!language_classifier_train_epoch(trainer, filename, NULL, LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES, minibatch_size)) {
            log_error("Error in epoch\n");
            logistic_regression_trainer_destroy(trainer);
            exit(EXIT_FAILURE);
        }
    }

    double final_cost = compute_total_cost(trainer, cost_file, cost_batches);

    *diverged = final_cost > initial_cost;
    log_info("final_cost = %f, initial_cost = %f\n", final_cost, initial_cost);

    return final_cost;
}

typedef struct language_classifier_sgd_params {
    double lambda;
    double gamma_0;
} language_classifier_sgd_params_t;

typedef struct language_classifier_ftrl_params {
    double alpha;
    double lambda1;
    double lambda2;
} language_classifier_ftrl_params_t;

VECTOR_INIT(language_classifier_sgd_param_array, language_classifier_sgd_params_t)
VECTOR_INIT(language_classifier_ftrl_param_array, language_classifier_ftrl_params_t)

/* Uses the one standard-error rule (http://www.stat.cmu.edu/~ryantibs/datamining/lectures/19-val2.pdf)
   A solution that's better regularized is preferred if it's within one standard error
   of the solution with the lowest cross-validation error.
*/

language_classifier_sgd_params_t language_classifier_parameter_sweep_sgd(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, size_t minibatch_size) {
    double best_cost = DBL_MAX;

    double default_lambda = 0.0;
    size_t lambda_schedule_size = 0;
    double *lambda_schedule = NULL;

    sgd_trainer_t *sgd = trainer->optimizer.sgd;

    if (sgd->reg_type == REGULARIZATION_L2) {
        default_lambda = DEFAULT_L2;
        lambda_schedule_size = L2_SCHEDULE_SIZE;
        lambda_schedule = L2_SCHEDULE;
    } else if (sgd->reg_type == REGULARIZATION_L1) {
        lambda_schedule_size = L1_SCHEDULE_SIZE;
        lambda_schedule = L1_SCHEDULE;
        default_lambda = DEFAULT_L1;
    }

    double_array *costs = double_array_new();
    language_classifier_sgd_param_array *all_params = language_classifier_sgd_param_array_new();

    language_classifier_sgd_params_t best_params = (language_classifier_sgd_params_t){default_lambda, DEFAULT_GAMMA_0};
    double cost;
    language_classifier_sgd_params_t params;

    cartesian_product_iterator_t *iter = cartesian_product_iterator_new(2, lambda_schedule_size, GAMMA_SCHEDULE_SIZE);
    for (uint32_t *vals = cartesian_product_iterator_start(iter); !cartesian_product_iterator_done(iter); vals = cartesian_product_iterator_next(iter)) {
        double lambda = lambda_schedule[vals[0]];
        double gamma_0 = GAMMA_SCHEDULE[vals[1]];

        params.lambda = lambda,
        params.gamma_0 = gamma_0;

        if (!logistic_regression_trainer_reset_params_sgd(trainer, lambda, gamma_0)) {
            log_error("Error resetting params\n");
            logistic_regression_trainer_destroy(trainer);
            exit(EXIT_FAILURE);
        }

        log_info("Optimizing hyperparameters. Trying lambda=%.7f, gamma_0=%f\n", lambda, gamma_0);

        bool diverged = false;
        cost = language_classifier_cv_cost(trainer, filename, cv_filename, minibatch_size, &diverged);

        if (!diverged) {
            language_classifier_sgd_param_array_push(all_params, params);
            double_array_push(costs, cost);
        } else {
            log_info("Diverged, cost = %f\n", cost);
        }

        log_info("Total cost = %f\n", cost);
        if (cost < best_cost) {
            log_info("Better than current best parameters: setting lambda=%.7f, gamma_0=%f\n", lambda, gamma_0);
            best_cost = cost;
            best_params.lambda = lambda;
            best_params.gamma_0 = gamma_0;
        }
    }

    size_t num_params = costs->n;
    if (num_params > 0) {
        language_classifier_sgd_params_t *param_values = all_params->a;
        double *cost_values = costs->a;

        double std_error = double_array_std(cost_values, num_params) / sqrt((double)num_params);

        double max_cost = best_cost + std_error;
        log_info("max_cost = %f using the one standard error rule\n", max_cost);

        for (size_t i = 0; i < num_params; i++) {
            cost = cost_values[i];
            params = param_values[i];

            if (cost < max_cost && params.lambda > best_params.lambda) {
                best_params = params;
                log_info("cost (%f) < max_cost and better regularized, setting lambda=%.7f, gamma_0=%f\n", cost, params.lambda, params.gamma_0);
            }
        }
    }

    language_classifier_sgd_param_array_destroy(all_params);
    double_array_destroy(costs);

    return best_params;
}


language_classifier_ftrl_params_t language_classifier_parameter_sweep_ftrl(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, size_t minibatch_size) {
    double best_cost = DBL_MAX;

    language_classifier_ftrl_params_t best_params = (language_classifier_ftrl_params_t){DEFAULT_ALPHA, DEFAULT_L1, DEFAULT_L2};

    double_array *costs = double_array_new();
    language_classifier_ftrl_param_array *all_params = language_classifier_ftrl_param_array_new();
    language_classifier_ftrl_params_t params;
    double cost;

    cartesian_product_iterator_t *iter = cartesian_product_iterator_new(3, L1_SCHEDULE_SIZE, L2_SCHEDULE_SIZE, ALPHA_SCHEDULE_SIZE);
    for (uint32_t *vals = cartesian_product_iterator_start(iter); !cartesian_product_iterator_done(iter); vals = cartesian_product_iterator_next(iter)) {
        double lambda1 = L1_SCHEDULE[vals[0]];
        double lambda2 = L2_SCHEDULE[vals[1]];
        double alpha = ALPHA_SCHEDULE[vals[2]];

        params.lambda1 = lambda1,
        params.lambda2 = lambda2,
        params.alpha = alpha;

        if (!logistic_regression_trainer_reset_params_ftrl(trainer, alpha, DEFAULT_BETA, lambda1, lambda2)) {
            log_error("Error resetting params\n");
            logistic_regression_trainer_destroy(trainer);
            exit(EXIT_FAILURE);
        }

        log_info("Optimizing hyperparameters. Trying lambda1=%.7f, lambda2=%.7f, alpha=%f\n", lambda1, lambda2, alpha);

        bool diverged = false;
        cost = language_classifier_cv_cost(trainer, filename, cv_filename, minibatch_size, &diverged);

        if (!diverged) {
            language_classifier_ftrl_param_array_push(all_params, params);
            double_array_push(costs, cost);
        } else {
            log_info("Diverged, cost = %f\n", cost);
        }

        log_info("Total cost = %f\n", cost);
        if (cost < best_cost) {
            log_info("Better than current best parameters: setting lambda1=%.7f, lambda2=%.7f, alpha=%f\n", lambda1, lambda2, alpha);
            best_cost = cost;
            best_params.lambda1 = lambda1;
            best_params.lambda2 = lambda2;
            best_params.alpha = alpha;
        }
    }

    size_t num_params = costs->n;
    if (num_params > 0) {
        language_classifier_ftrl_params_t *param_values = all_params->a;
        double *cost_values = costs->a;

        double std_error = double_array_std(cost_values, num_params) / sqrt((double)num_params);

        double max_cost = best_cost + std_error;
        log_info("best_cost = %f, std_error = %f, max_cost = %f using the one standard error rule\n", best_cost, std_error, max_cost);

        for (size_t i = 0; i < num_params; i++) {
            cost = cost_values[i];
            params = param_values[i];

            log_info("cost = %f, lambda1 = %f, lambda2 = %f, alpha = %f\n", cost, params.lambda1, params.lambda2, params.alpha);

            if (cost < max_cost &&
                (params.lambda1 > best_params.lambda1 || double_equals(params.lambda1, best_params.lambda1)) &&
                (params.lambda2 > best_params.lambda2 || double_equals(params.lambda2, best_params.lambda2))
               ) {
                if (double_equals(params.lambda1, best_params.lambda1) && double_equals(params.lambda2, best_params.lambda2) && params.alpha > best_params.alpha) {
                    log_info("cost < max_cost but higher alpha\n");
                    continue;
                }
                best_params = params;
                log_info("cost (%f) < max_cost and better regularized, setting lambda1=%.7f, lambda2=%.7f alpha=%f\n", cost, params.lambda1, params.lambda2, params.alpha);
            }
        }
    }

    language_classifier_ftrl_param_array_destroy(all_params);
    double_array_destroy(costs);

    return best_params;
}


static language_classifier_t *trainer_finalize(logistic_regression_trainer_t *trainer, char *test_filename) {
    if (trainer == NULL) return NULL;

    log_info("Done training\n");

    if (test_filename != NULL) {
        double test_accuracy = compute_cv_accuracy(trainer, test_filename);
        log_info("Test accuracy = %f\n", test_accuracy);
    }

    language_classifier_t *classifier = language_classifier_new();
    if (classifier == NULL) {
        log_error("Error creating classifier\n");
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    // Reassign weights and features to the classifier model
    // final_weights

    if (trainer->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        sgd_trainer_t *sgd_trainer = trainer->optimizer.sgd;
        if (sgd_trainer->reg_type == REGULARIZATION_L2 || sgd_trainer->reg_type == REGULARIZATION_NONE) {
            double_matrix_t *weights = logistic_regression_trainer_final_weights(trainer);
            classifier->weights_type = MATRIX_DENSE;
            classifier->weights.dense = weights;
        } else if (sgd_trainer->reg_type == REGULARIZATION_L1) {
            sparse_matrix_t *sparse_weights = logistic_regression_trainer_final_weights_sparse(trainer);
            classifier->weights_type = MATRIX_SPARSE;
            classifier->weights.sparse = sparse_weights;
            log_info("Weights sparse: %zu rows (m=%u), %" PRIu32 " cols, %zu elements\n", sparse_weights->indptr->n, sparse_weights->m, sparse_weights->n, sparse_weights->data->n);
        }
    } else if (trainer->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        sparse_matrix_t *sparse_weights = logistic_regression_trainer_final_weights_sparse(trainer);
        classifier->weights_type = MATRIX_SPARSE;
        classifier->weights.sparse = sparse_weights;
        log_info("Weights sparse: %zu rows (m=%u), %" PRIu32 " cols, %zu elements\n", sparse_weights->indptr->n, sparse_weights->m, sparse_weights->n, sparse_weights->data->n);
    }


    classifier->num_features = trainer->num_features;
    classifier->features = trainer->feature_ids;
    // Set trainer feature_ids to NULL so it doesn't get destroyed
    trainer->feature_ids = NULL;

    size_t num_labels = trainer->num_labels;
    classifier->num_labels = num_labels;

    char **strings = malloc(sizeof(char *) * num_labels);

    const char *label;
    uint32_t label_id;
    kh_foreach(trainer->label_ids, label, label_id, {
        if (label_id >= num_labels) {
            log_error("label_id %d >= num_labels %zu\n", label_id, num_labels);
            exit(EXIT_FAILURE);
        }
        strings[label_id] = (char *)label;
    })

    classifier->labels = cstring_array_from_strings(strings, num_labels);

    for (size_t i = 0; i < num_labels; i++) {
        free(strings[i]);
    }

    free(strings);

    logistic_regression_trainer_destroy(trainer);
    return classifier;
}


language_classifier_t *language_classifier_train_sgd(char *filename, char *subset_filename, bool cross_validation_set, char *cv_filename, char *test_filename, uint32_t num_iterations, size_t minibatch_size, regularization_type_t reg_type) {
    logistic_regression_trainer_t *trainer = language_classifier_init_sgd_reg(filename, minibatch_size, reg_type);

    language_classifier_sgd_params_t params = language_classifier_parameter_sweep_sgd(trainer, subset_filename, cv_filename, minibatch_size);
    log_info("Best params: lambda=%f, gamma_0=%f\n", params.lambda, params.gamma_0);

    if (!logistic_regression_trainer_reset_params_sgd(trainer, params.lambda, params.gamma_0)) {
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    /* If there's not a distinct cross-validation set, e.g.
       when training the production model, then the cross validation
       file is just a subset of the training data and only used
       for setting the hyperparameters, so ignore it after we're
       done with the parameter sweep.
    */
    if (!cross_validation_set) {
        cv_filename = NULL;
    }

    for (uint32_t epoch = 0; epoch < num_iterations; epoch++) {
        log_info("Doing epoch %d\n", epoch);

        trainer->epochs = epoch;

        if (!language_classifier_train_epoch(trainer, filename, cv_filename, -1, minibatch_size)) {
            log_error("Error in epoch\n");
            logistic_regression_trainer_destroy(trainer);
            return NULL;
        }
    }

    return trainer_finalize(trainer, test_filename);
}

language_classifier_t *language_classifier_train_ftrl(char *filename, char *subset_filename, bool cross_validation_set, char *cv_filename, char *test_filename, uint32_t num_iterations, size_t minibatch_size) {
    logistic_regression_trainer_t *trainer = language_classifier_init_ftrl(filename, minibatch_size);

    language_classifier_ftrl_params_t params = language_classifier_parameter_sweep_ftrl(trainer, subset_filename, cv_filename, minibatch_size);
    log_info("Best params: lambda1=%.7f, lambda2=%.7f, alpha=%f\n", params.lambda1, params.lambda2, params.alpha);

    if (!logistic_regression_trainer_reset_params_ftrl(trainer, params.alpha, DEFAULT_BETA, params.lambda1, params.lambda2)) {
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    /* If there's not a distinct cross-validation set, e.g.
       when training the production model, then the cross validation
       file is just a subset of the training data and only used
       for setting the hyperparameters, so ignore it after we're
       done with the parameter sweep.
    */
    if (!cross_validation_set) {
        cv_filename = NULL;
    }

    for (uint32_t epoch = 0; epoch < num_iterations; epoch++) {
        log_info("Doing epoch %d\n", epoch);

        trainer->epochs = epoch;

        if (!language_classifier_train_epoch(trainer, filename, cv_filename, -1, minibatch_size)) {
            log_error("Error in epoch\n");
            logistic_regression_trainer_destroy(trainer);
            return NULL;
        }
    }

    return trainer_finalize(trainer, test_filename);
}



typedef enum {
    LANGUAGE_CLASSIFIER_TRAIN_POSITIONAL_ARG,
    LANGUAGE_CLASSIFIER_TRAIN_ARG_ITERATIONS,
    LANGUAGE_CLASSIFIER_TRAIN_ARG_OPTIMIZER,
    LANGUAGE_CLASSIFIER_TRAIN_ARG_REGULARIZATION,
    LANGUAGE_CLASSIFIER_TRAIN_ARG_MINIBATCH_SIZE
} language_classifier_train_keyword_arg_t;

#define LANGUAGE_CLASSIFIER_TRAIN_USAGE "Usage: ./language_classifier_train [train|cv] filename [cv_filename] [test_filename] [output_dir] [--iterations number --opt (sgd|ftrl) --reg (l1|l2) --minibatch-size number]\n"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    int pos_args = 1;

    language_classifier_train_keyword_arg_t kwarg = LANGUAGE_CLASSIFIER_TRAIN_POSITIONAL_ARG;

    size_t num_epochs = TRAIN_EPOCHS;
    size_t minibatch_size = LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE;
    logistic_regression_optimizer_type optim_type = LOGISTIC_REGRESSION_OPTIMIZER_SGD;
    regularization_type_t reg_type = REGULARIZATION_L2;

    size_t position = 0;

    ssize_t arg_iterations;
    ssize_t arg_minibatch_size;

    char *command = NULL;
    char *filename = NULL;

    char *cv_filename = NULL;
    char *test_filename = NULL;

    bool cross_validation_set = false;

    char *output_dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;

    for (int i = pos_args; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--iterations")) {
            kwarg = LANGUAGE_CLASSIFIER_TRAIN_ARG_ITERATIONS;
            continue;
        }

        if (string_equals(arg, "--opt")) {
            kwarg = LANGUAGE_CLASSIFIER_TRAIN_ARG_OPTIMIZER;
            continue;
        }

        if (string_equals(arg, "--reg")) {
            kwarg = LANGUAGE_CLASSIFIER_TRAIN_ARG_REGULARIZATION;
            continue;
        }

        if (string_equals(arg, "--minibatch-size")) {
            kwarg = LANGUAGE_CLASSIFIER_TRAIN_ARG_MINIBATCH_SIZE;
            continue;
        }

        if (kwarg == LANGUAGE_CLASSIFIER_TRAIN_ARG_ITERATIONS) {
            if (sscanf(arg, "%zd", &arg_iterations) != 1 || arg_iterations < 0) {
                log_error("Bad arg for --iterations: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            num_epochs = (size_t)arg_iterations;
        } else if (kwarg == LANGUAGE_CLASSIFIER_TRAIN_ARG_OPTIMIZER) {
            if (string_equals(arg, "sgd")) {
                optim_type = LOGISTIC_REGRESSION_OPTIMIZER_SGD;
            } else if (string_equals(arg, "ftrl")) {
                log_info("ftrl\n");
                optim_type = LOGISTIC_REGRESSION_OPTIMIZER_FTRL;
            } else {
                log_error("Bad arg for --opt: %s\n", arg);
                exit(EXIT_FAILURE);
            }
        } else if (kwarg == LANGUAGE_CLASSIFIER_TRAIN_ARG_REGULARIZATION) {
            if (string_equals(arg, "l2")) {
                reg_type = REGULARIZATION_L2;
            } else if (string_equals(arg, "l1"))  {
                reg_type = REGULARIZATION_L1;
            } else {
                log_error("Bad arg for --reg: %s\n", arg);
                exit(EXIT_FAILURE);
            }
        } else if (kwarg == LANGUAGE_CLASSIFIER_TRAIN_ARG_MINIBATCH_SIZE) {
            if (sscanf(arg, "%zd", &arg_minibatch_size) != 1 || arg_minibatch_size < 0) {
                log_error("Bad arg for --batch: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            minibatch_size = (size_t)arg_minibatch_size;
        } else if (position == 0) {
            command = arg;
            if (string_equals(command, "cv")) {
                cross_validation_set = true;
            } else if (!string_equals(command, "train")) {
                printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
                exit(EXIT_FAILURE);
            }
            position++;
        } else if (position == 1) {
            filename = arg;
            position++;
        } else if (position == 2 && cross_validation_set) {
            cv_filename = arg;
            position++;
        } else if (position == 2 && !cross_validation_set) {
            output_dir = arg;
            position++;
        } else if (position == 3 && cross_validation_set) {
            test_filename = arg;
            position++;
        } else if (position == 4 && cross_validation_set) {
            output_dir = arg;
            position++;
        }
        kwarg = LANGUAGE_CLASSIFIER_TRAIN_POSITIONAL_ARG;
    }

    if ((command == NULL || filename == NULL) || (cross_validation_set && (cv_filename == NULL || test_filename == NULL))) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    #if !defined(HAVE_SHUF) && !defined(HAVE_GSHUF)
    log_warn("shuf must be installed to train address parser effectively. If this is a production machine, please install shuf. No shuffling will be performed.\n");
    #endif

    if (!address_dictionary_module_setup(NULL)) {
        log_error("Could not load address dictionaries\n");
        exit(EXIT_FAILURE);
    } else if (!transliteration_module_setup(NULL)) {
        log_error("Could not load transliteration module\n");
        exit(EXIT_FAILURE);
    }

    char_array *temp_file = char_array_new();
    char_array_cat_printf(temp_file, "%s.tmp", filename);

    char *temp_filename = char_array_get_string(temp_file);

    char_array *head_command = char_array_new();

    size_t subset_examples = LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES * LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE;

    char_array_cat_printf(head_command, "head -n %d %s > %s", subset_examples, filename, temp_filename);
    int ret = system(char_array_get_string(head_command));

    if (ret != 0) {
        exit(EXIT_FAILURE);
    }

    char_array *temp_cv_file = NULL;
    if (!cross_validation_set) {
        char_array_clear(head_command);

        temp_cv_file = char_array_new();
        char_array_cat_printf(temp_cv_file, "%s.cv.tmp", filename);

        char *temp_cv_filename = char_array_get_string(temp_cv_file);

        char_array_cat_printf(head_command, "head -n %d %s | tail -n %d > %s", subset_examples * 2, filename, subset_examples, temp_cv_filename);
        int ret = system(char_array_get_string(head_command));

        cv_filename = temp_cv_filename;
    }

    if (ret != 0) {
        exit(EXIT_FAILURE);
    }

    char_array_destroy(head_command);

    language_classifier_t *language_classifier = NULL;

    if (optim_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        language_classifier = language_classifier_train_sgd(filename, temp_filename, cross_validation_set, cv_filename, test_filename, num_epochs, minibatch_size, reg_type);
    } else if (optim_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        language_classifier = language_classifier_train_ftrl(filename, temp_filename, cross_validation_set, cv_filename, test_filename, num_epochs, minibatch_size);
    }

    remove(temp_filename);
    char_array_destroy(temp_file);
    if (temp_cv_file != NULL) {
        char_array_destroy(temp_cv_file);
    }

    log_info("Done with classifier\n");
    char_array *path = char_array_new_size(strlen(output_dir) + PATH_SEPARATOR_LEN + strlen(LANGUAGE_CLASSIFIER_FILENAME));

    char *classifier_path;
    if (language_classifier != NULL) {
        char_array_cat_joined(path, PATH_SEPARATOR, true, 2, output_dir, LANGUAGE_CLASSIFIER_FILENAME);
        classifier_path = char_array_get_string(path);

        language_classifier_save(language_classifier, classifier_path);
        language_classifier_destroy(language_classifier);
    }

    char_array_destroy(path);

    log_info("Success!\n");

    address_dictionary_module_teardown();

}
