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

#define LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD 1.0
#define LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD 1

#define LOG_BATCH_INTERVAL 10
#define COMPUTE_COST_INTERVAL 100

#define TRAIN_EPOCHS 10

logistic_regression_trainer_t *language_classifier_init_thresholds(char *filename, bool with_country, double feature_count_threshold, uint32_t label_count_threshold) {
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
    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, with_country)) != NULL) {
        if (!count_features_minibatch(feature_counts, minibatch->features, true)){
            log_error("Counting minibatch features failed\n");
            exit(EXIT_FAILURE);
        }

        if (!count_labels_minibatch(label_counts, minibatch->labels)) {
            log_error("Counting minibatch labeles failed\n");
            exit(EXIT_FAILURE);
        }

        if (num_batches % LOG_BATCH_INTERVAL == 0) {
            log_info("Counted %zu batches\n", num_batches);
        }

        num_batches++;

        language_classifier_minibatch_destroy(minibatch);
    }
    log_info("Done counting, finalizing\n");

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

    khash_t(str_uint32) *label_ids = select_labels_threshold(label_counts, label_count_threshold);
    if (label_ids == NULL) {
        log_error("Error creating labels\n");
        exit(EXIT_FAILURE);
    }

    // Don't free the label strings as the pointers are reused in select_labels_threshold
    kh_destroy(str_uint32, label_counts);

    return logistic_regression_trainer_init(feature_ids, label_ids);
}

logistic_regression_trainer_t *language_classifier_init(char *filename, bool with_country) {
    return language_classifier_init_thresholds(filename, with_country, LANGUAGE_CLASSIFIER_FEATURE_COUNT_THRESHOLD, LANGUAGE_CLASSIFIER_LABEL_COUNT_THRESHOLD);
}

double compute_cv_accuracy(logistic_regression_trainer_t *trainer, char *filename, bool with_country) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    uint32_t correct = 0;
    uint32_t total = 0;

    matrix_t *p_y = matrix_new_zeros(LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE, trainer->num_labels);

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, with_country)) != NULL) {
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

bool language_classifier_train_epoch(logistic_regression_trainer_t *trainer, char *filename, char *cv_filename, bool with_country) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return false;
    }

    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);

    language_classifier_minibatch_t *minibatch;

    size_t num_batches = 0;
    double batch_cost = 0.0;
    double total_cost = 0.0;
    double last_cost = 0.0;

    double train_cost = 0.0;
    double cv_accuracy = 0.0;

    while ((minibatch = language_classifier_data_set_get_minibatch(data_set, with_country)) != NULL) {
        bool compute_cost = num_batches % COMPUTE_COST_INTERVAL == 0 && num_batches > 0;
        
        if (num_batches % LOG_BATCH_INTERVAL == 0 && num_batches > 0) {
            log_info("Epoch %u, trained %zu batches\n", trainer->epochs, num_batches);
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
            cv_accuracy = compute_cv_accuracy(trainer, cv_filename, with_country);
            log_info("cv accuracy=%f\n", cv_accuracy);
        }

        num_batches++;

        language_classifier_minibatch_destroy(minibatch);
    }

    language_classifier_data_set_destroy(data_set);

    return true;
}


language_classifier_t *language_classifier_train(char *filename, char *cv_filename, uint32_t num_iterations, bool with_country) {
    logistic_regression_trainer_t *trainer = language_classifier_init(filename, with_country);

    for (uint32_t epoch = 0; epoch < num_iterations; epoch++) {
        log_info("Doing epoch %d\n", epoch);

        trainer->epochs = epoch;

        #if defined(HAVE_SHUF)
        log_info("Shuffling\n");

        if (!shuffle_file(filename)) {
            log_error("Error in shuffle\n");
            logistic_regression_trainer_destroy(trainer);
            return NULL;
        }

        log_info("Shuffle complete\n");
        #endif
        
        if (!language_classifier_train_epoch(trainer, filename, cv_filename, with_country)) {
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

    language_classifier_t *classifier = language_classifier_new();

    // Reassign weights and features to the classifier model
    classifier->weights = trainer->weights;
    trainer->weights = NULL;

    classifier->num_features = trainer->num_features;
    classifier->features = trainer->feature_ids;
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


#define LANGUAGE_CLASSIFIER_TRAIN_USAGE "Usage: ./address_parser_train [train|cv] filename [cv_filename] [output_dir]\n"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    char *command = argv[1];
    bool cross_validate = false;

    if (string_equals(command, "cv")) {
        cross_validate = true;
    } else if (!string_equals(command, "train")) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[2];
    char *cv_filename = NULL;

    if (cross_validate && argc < 4) {
        printf(LANGUAGE_CLASSIFIER_TRAIN_USAGE);
        exit(EXIT_FAILURE);
    } else if (cross_validate) {
        cv_filename = argv[3];
    }

    char *output_dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;
    int output_dir_arg = cross_validate ? 4 : 3;

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

    language_classifier_t *language_classifier = language_classifier_train(filename, cv_filename, TRAIN_EPOCHS, false);

    log_info("Done with classifier\n");
    char_array *path = char_array_new_size(strlen(output_dir) + PATH_SEPARATOR_LEN + strlen(LANGUAGE_CLASSIFIER_COUNTRY_FILENAME)); 

    char *classifier_path;
    if (language_classifier != NULL) {
        char_array_cat_joined(path, PATH_SEPARATOR, true, 2, output_dir, LANGUAGE_CLASSIFIER_FILENAME);
        classifier_path = char_array_get_string(path);

        language_classifier_save(language_classifier, classifier_path);
        language_classifier_destroy(language_classifier);
    }

    language_classifier_t *language_classifier_country = language_classifier_train(filename, cv_filename, TRAIN_EPOCHS, true);

    if (language_classifier_country != NULL) {
        char_array_clear(path);
        char_array_cat_joined(path, PATH_SEPARATOR, true, 2, output_dir, LANGUAGE_CLASSIFIER_COUNTRY_FILENAME);

        classifier_path = char_array_get_string(path);

        language_classifier_save(language_classifier_country, classifier_path);
        language_classifier_destroy(language_classifier_country);
    }

    char_array_destroy(path);

    log_info("Success!\n");

    address_dictionary_module_teardown();

}
