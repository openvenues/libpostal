#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "log/log.h"
#include "address_dictionary.h"
#include "language_classifier.h"
#include "language_classifier_io.h"
#include "logistic_regression.h"
#include "logistic_regression_trainer.h"
#include "shuffle.h"
#include "sparse_matrix.h"
#include "sparse_matrix_utils.h"

#define LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD 1.0
#define LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD 100

#define LOG_BATCH_INTERVAL 10
#define COMPUTE_COST_INTERVAL 100

#define LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES 100

static double GAMMA_SCHEDULE[] = {0.01, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
static const size_t GAMMA_SCHEDULE_SIZE = sizeof(GAMMA_SCHEDULE) / sizeof(double);

#define DEFAULT_GAMMA_0 10.0

static  double LAMBDA_SCHEDULE[]  = {0.0, 1e-5, 1e-4, 0.001, 0.01, 0.1, \
                                     0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
static const size_t LAMBDA_SCHEDULE_SIZE  = sizeof(LAMBDA_SCHEDULE) / sizeof(double);

#define DEFAULT_LAMBDA 0.0

#define TRAIN_EPOCHS 10

#define HYPERPARAMETER_EPOCHS 30

logistic_regression_trainer_t *language_classifier_init_thresholds(char *filename, double feature_count_threshold, uint32_t label_count_threshold) {
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
    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, NULL)) != NULL) {
        if (!count_labels_minibatch(label_counts, minibatch->labels)) {
            log_error("Counting minibatch labeles failed\n");
            exit(EXIT_FAILURE);
        }

        if (num_batches % LOG_BATCH_INTERVAL == 0) {
            log_info("Counting labels, did %zu batches\n", num_batches);
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

        if (num_batches % LOG_BATCH_INTERVAL == 0) {
            log_info("Counting features, did %zu batches\n", num_batches);
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


    return logistic_regression_trainer_init(feature_ids, label_ids, DEFAULT_GAMMA_0, DEFAULT_LAMBDA);
}

logistic_regression_trainer_t *language_classifier_init(char *filename) {
    return language_classifier_init_thresholds(filename, LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD, LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD);
}

double compute_cv_accuracy(logistic_regression_trainer_t *trainer, char *filename) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    uint32_t correct = 0;
    uint32_t total = 0;

    matrix_t *p_y = matrix_new_zeros(LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE, trainer->num_labels);

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, trainer->label_ids)) != NULL) {
        sparse_matrix_t *x = feature_matrix(trainer->feature_ids, minibatch->features);
        uint32_array *y = label_vector(trainer->label_ids, minibatch->labels);

        matrix_resize(p_y, x->m, trainer->num_labels);

        if (!logistic_regression_model_expectation(trainer->weights, x, p_y)) {
            log_error("Predict cv batch failed\n");
            exit(EXIT_FAILURE);
        }

        double *row;
        for (size_t i = 0; i < p_y->m; i++) {
            row = matrix_get_row(p_y, i);

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
    matrix_destroy(p_y);

    double accuracy = (double)correct / total;
    return accuracy;
}



double compute_total_cost(logistic_regression_trainer_t *trainer, char *filename, ssize_t compute_batches) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    double total_cost = 0.0;
    size_t num_batches = 0;

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, trainer->label_ids)) != NULL) {

        double batch_cost = logistic_regression_trainer_batch_cost(trainer, minibatch->features, minibatch->labels);
        total_cost += batch_cost;

        language_classifier_minibatch_destroy(minibatch);

        num_batches++;

        if (compute_batches > 0 && num_batches == (size_t)compute_batches) {
            break;
        }
    }

    language_classifier_data_set_destroy(data_set);

    return total_cost;
}


bool language_classifier_train_epoch(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, ssize_t train_batches) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return false;
    }

    #if defined(HAVE_SHUF)
    log_info("Shuffling\n");

    if (!shuffle_file(filename)) {
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

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, trainer->label_ids)) != NULL) {
        bool compute_cost = num_batches % COMPUTE_COST_INTERVAL == 0 && num_batches > 0;
        
        if (num_batches % LOG_BATCH_INTERVAL == 0 && num_batches > 0) {
            log_info("Epoch %u, doing batch %zu\n", trainer->epochs, num_batches);
        }

        if (compute_cost) {
            train_cost = logistic_regression_trainer_batch_cost(trainer, minibatch->features, minibatch->labels);
            log_info("cost = %f\n", train_cost);
        }

        if (!logistic_regression_trainer_train_batch(trainer, minibatch->features, minibatch->labels)){
            log_error("Train batch failed\n");
            exit(EXIT_FAILURE);
        }

        if (compute_cost && cv_filename != NULL) {
            cv_accuracy = compute_cv_accuracy(trainer, cv_filename);
            log_info("cv accuracy=%f\n", cv_accuracy);
        }

        num_batches++;

        if (train_batches > 0 && num_batches == (size_t)train_batches) {
            log_info("Epoch %u, trained %zu batches\n", trainer->epochs, num_batches);
            train_cost = logistic_regression_trainer_batch_cost(trainer, minibatch->features, minibatch->labels);
            log_info("cost = %f\n", train_cost);
            break;
        }

        language_classifier_minibatch_destroy(minibatch);

    }

    language_classifier_data_set_destroy(data_set);

    return true;
}

typedef struct language_classifier_params {
    double lambda;
    double gamma_0;
} language_classifier_params_t;

language_classifier_params_t language_classifier_parameter_sweep(char *filename, char *cv_filename) {
    // Select features using the full data set
    logistic_regression_trainer_t *trainer = language_classifier_init(filename);

    double best_cost = 0.0;

    language_classifier_params_t best_params = (language_classifier_params_t){0.0, 0.0};

    for (size_t i = 0; i < LAMBDA_SCHEDULE_SIZE; i++) {
        for (size_t j = 0; j < GAMMA_SCHEDULE_SIZE; j++) {
            trainer->lambda = LAMBDA_SCHEDULE[i];
            trainer->gamma_0 = GAMMA_SCHEDULE[j];

            log_info("Optimizing hyperparameters. Trying lambda=%f, gamma_0=%f\n", trainer->lambda, trainer->gamma_0);

            for (int k = 0; k < HYPERPARAMETER_EPOCHS; k++) {
                trainer->epochs = k;

                if (!language_classifier_train_epoch(trainer, filename, NULL, LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES)) {
                    log_error("Error in epoch\n");
                    logistic_regression_trainer_destroy(trainer);
                    exit(EXIT_FAILURE);
                }
            }

            ssize_t cost_batches;
            char *cost_file;

            if (cv_filename == NULL) {
                cost_file = filename;
                cost_batches = LANGUAGE_CLASSIFIER_HYPERPARAMETER_BATCHES;
            } else {
                cost_file = cv_filename;
                cost_batches = -1;
            }

            double cost = compute_total_cost(trainer, cost_file, cost_batches);
            log_info("Total cost = %f\n", cost);
            if ((i == 0 && j == 0) || cost < best_cost) {
                log_info("Better than current best parameters: lambda=%f, gamma_0=%f\n", trainer->lambda, trainer->gamma_0);
                best_cost = cost;
                best_params.lambda = trainer->lambda;
                best_params.gamma_0 = trainer->gamma_0;
            }
        }
    }

    return best_params;
}


language_classifier_t *language_classifier_train(char *filename, char *subset_filename, bool cross_validation_set, char *cv_filename, char *test_filename, uint32_t num_iterations) {
    language_classifier_params_t params = language_classifier_parameter_sweep(subset_filename, cv_filename);
    log_info("Best params: lambda=%f, gamma_0=%f\n", params.lambda, params.gamma_0);
    
    logistic_regression_trainer_t *trainer = language_classifier_init(filename);
    trainer->lambda = params.lambda;
    trainer->gamma_0 = params.gamma_0;

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
        
        if (!language_classifier_train_epoch(trainer, filename, cv_filename, -1)) {
            log_error("Error in epoch\n");
            logistic_regression_trainer_destroy(trainer);
            return NULL;
        }
    }

    log_info("Done training\n");

    if (!logistic_regression_trainer_finalize(trainer)) {
        log_error("Error in finalization\n");
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

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
    classifier->weights = trainer->weights;
    trainer->weights = NULL;

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


#define LANGUAGE_CLASSIFIER_TRAIN_USAGE "Usage: ./language_classifier_train [train|cv] filename [cv_filename] [test_filename] [output_dir]\n"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    char *command = argv[1];
    bool cross_validation_set = false;

    if (string_equals(command, "cv")) {
        cross_validation_set = true;
    } else if (!string_equals(command, "train")) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[2];
    char *cv_filename = NULL;
    char *test_filename = NULL;

    if (cross_validation_set && argc < 5) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    } else if (cross_validation_set) {
        cv_filename = argv[3];
        test_filename = argv[4];
    }

    char *output_dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;
    int output_dir_arg = cross_validation_set ? 5 : 3;

    if (argc > output_dir_arg) {
        output_dir = argv[output_dir_arg];
    }

    #if !defined(HAVE_SHUF)
    log_warn("shuf must be installed to train address parser effectively. If this is a production machine, please install shuf. No shuffling will be performed.\n");
    #endif

    if (!address_dictionary_module_setup(NULL)) {
        log_error("Could not load address dictionaries\n");
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

    language_classifier_t *language_classifier = language_classifier_train(filename, temp_filename, cross_validation_set, cv_filename, test_filename, TRAIN_EPOCHS);

    remove(temp_filename);
    char_array_destroy(temp_file);
    if (temp_cv_file != NULL) {
        char_array_destroy(temp_cv_file);
    }

    log_info("Done with classifier\n");
    char_array *path = char_array_new_size(strlen(output_dir) + PATH_SEPARATOR_LEN + strlen(LANGUAGE_CLASSIFIER_COUNTRY_FILENAME)); 

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
