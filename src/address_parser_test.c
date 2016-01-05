#include "address_parser.h"
#include "address_parser_io.h"
#include "address_dictionary.h"
#include "averaged_perceptron_trainer.h"
#include "collections.h"
#include "constants.h"
#include "file_utils.h"
#include "geodb.h"
#include "normalize.h"

#include "log/log.h"

typedef struct address_parser_test_results {
    size_t num_errors;
    size_t num_predictions;
    size_t num_address_errors;
    size_t num_address_predictions;
    uint32_t *confusion;
} address_parser_test_results_t;


uint32_t get_class_index(address_parser_t *parser, char *name) {
    uint32_t i;
    char *str;

    cstring_array_foreach(parser->model->classes, i, str, {
        if (strcmp(name, str) == 0) {
            return i;
        }
    })

    return parser->model->num_classes;
}

#define EMPTY_ADDRESS_PARSER_TEST_RESULT (address_parser_test_results_t){0, 0, 0, 0, NULL}

bool address_parser_test(address_parser_t *parser, char *filename, address_parser_test_results_t *result) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return NULL;
    }

    uint32_t num_classes = parser->model->num_classes;

    result->confusion = calloc(num_classes * num_classes, sizeof(uint32_t));

    address_parser_data_set_t *data_set = address_parser_data_set_init(filename);

    if (data_set == NULL) {
        log_error("Error initializing data set\n");
        return NULL;
    }

    address_parser_context_t *context = address_parser_context_new();

    bool success = false;

    size_t examples = 0;

    bool logged = false;

    while (address_parser_data_set_next(data_set)) {
        char *language = char_array_get_string(data_set->language);
        if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
            language = NULL;
        }
        char *country = char_array_get_string(data_set->country);

        address_parser_context_fill(context, parser, data_set->tokenized_str, language, country);

        cstring_array *token_labels = cstring_array_new_size(data_set->tokenized_str->strings->str->n);

        char *prev_label = NULL;

        address_parser_response_t *response = NULL;

        size_t starting_errors = result->num_errors;

        if (averaged_perceptron_tagger_predict(parser->model, parser, context, context->features, token_labels, &address_parser_features, data_set->tokenized_str)) {
            uint32_t i;
            char *predicted;
            cstring_array_foreach(token_labels, i, predicted, {
                char *truth = cstring_array_get_string(data_set->labels, i);

                if (strcmp(predicted, truth) != 0) {
                    result->num_errors++;

                    uint32_t predicted_index = get_class_index(parser, predicted);
                    uint32_t truth_index = get_class_index(parser, truth);

                    result->confusion[predicted_index * num_classes + truth_index]++;

                }
                result->num_predictions++;

            })

        }

        cstring_array_destroy(token_labels);

        if (result->num_errors > starting_errors) {
            result->num_address_errors++;
        }

        result->num_address_predictions++;

        if (result->num_address_predictions % 1000 == 0 && result->num_address_predictions > 0) {
            log_info("Did %zu examples\n", result->num_address_predictions);
        }

        tokenized_string_destroy(data_set->tokenized_str);
        data_set->tokenized_str = NULL;

    }

    address_parser_data_set_destroy(data_set);
    address_parser_context_destroy(context);

    return true;
}


int main(int argc, char **argv) {
    char *address_parser_dir = LIBPOSTAL_ADDRESS_PARSER_DIR;

    if (argc < 2) {
        log_error("Usage: ./address_parser_test filename [parser_dir]\n");
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];

    if (argc > 2) {
        address_parser_dir = argv[2];
    }

    if (!address_dictionary_module_setup(NULL)) {
        log_error("Could not load address dictionaries\n");
        exit(EXIT_FAILURE);
    }

    log_info("address dictionary module loaded\n");

    if (!geodb_module_setup(NULL)) {
        log_error("Could not load geodb dictionaries\n");
        exit(EXIT_FAILURE);
    }

    log_info("geodb module loaded\n");

    if (!address_parser_load(address_parser_dir)) {
        log_error("Could not initialize parser\n");
        exit(EXIT_FAILURE);
    }

    log_info("Finished initialization\n");

    address_parser_t *parser = get_address_parser();

    address_parser_test_results_t results = EMPTY_ADDRESS_PARSER_TEST_RESULT;

    if (!address_parser_test(parser, filename, &results)) {
        log_error("Error in training\n");
        exit(EXIT_FAILURE);
    }

    printf("Errors: %zu / %zu (%f%%)\n", results.num_errors, results.num_predictions, (double)results.num_errors / results.num_predictions);
    printf("Addresses: %zu / %zu (%f%%)\n\n", results.num_address_errors, results.num_address_predictions, (double)results.num_address_errors / results.num_address_predictions);


    printf("Confusion matrix:\n\n");
    uint32_t num_classes = parser->model->num_classes;
    for (uint32_t i = 0; i < num_classes; i++) {
        for (uint32_t j = 0; j < num_classes; j++) {
            if (i == j) {
                continue;
            }
            uint32_t class_errors = results.confusion[i * num_classes + j];

            if (class_errors > 0) {
                char *predicted = cstring_array_get_string(parser->model->classes, i);
                char *truth = cstring_array_get_string(parser->model->classes, j);

                printf("(%s, %s): %d\n", predicted, truth, class_errors);
            }
        }
    }

    free(results.confusion);

    address_parser_module_teardown();

    address_dictionary_module_teardown();
    geodb_module_teardown();
}
