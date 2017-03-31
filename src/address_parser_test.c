#include "address_parser.h"
#include "address_parser_io.h"
#include "address_dictionary.h"
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


static uint32_t address_parser_num_classes(address_parser_t *parser) {
    if (parser->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_t *ap = parser->model.ap;
        return parser->model.ap->num_classes;
    } else if (parser->model_type == ADDRESS_PARSER_TYPE_CRF) {
        return parser->model.crf->num_classes;
    }
    return 0;
}

static cstring_array *address_parser_class_strings(address_parser_t *parser) {
    if (parser->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        cstring_array *classes = parser->model.ap->classes;
        return parser->model.ap->classes;
    } else if (parser->model_type == ADDRESS_PARSER_TYPE_CRF) {
        return parser->model.crf->classes;
    }
    return NULL;
}

static uint32_t address_parser_get_class_index(address_parser_t *parser, char *name) {
    uint32_t i;
    char *str;

    cstring_array *classes = address_parser_class_strings(parser);
    uint32_t num_classes = address_parser_num_classes(parser);
    if (classes != NULL) {
        cstring_array_foreach(classes, i, str, {
            if (strcmp(name, str) == 0) {
                return i;
            }
        })
    }

    return num_classes;
}


#define EMPTY_ADDRESS_PARSER_TEST_RESULT (address_parser_test_results_t){0, 0, 0, 0, NULL}

bool address_parser_test(address_parser_t *parser, char *filename, address_parser_test_results_t *result, bool print_errors) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return NULL;
    }

    uint32_t num_classes = address_parser_num_classes(parser);

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

    cstring_array *token_labels = cstring_array_new();

    while (address_parser_data_set_next(data_set)) {
        char *language = char_array_get_string(data_set->language);
        if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
            language = NULL;
        }
        char *country = char_array_get_string(data_set->country);

        address_parser_context_fill(context, parser, data_set->tokenized_str, language, country);

        cstring_array_clear(token_labels);

        char *prev_label = NULL;

        size_t starting_errors = result->num_errors;

        bool prediction_success = address_parser_predict(parser, context, token_labels, &address_parser_features, data_set->tokenized_str);

        if (prediction_success) {
            uint32_t i;
            char *predicted;
            cstring_array_foreach(token_labels, i, predicted, {
                char *truth = cstring_array_get_string(data_set->labels, i);

                if (strcmp(predicted, truth) != 0) {
                    result->num_errors++;

                    uint32_t predicted_index = address_parser_get_class_index(parser, predicted);
                    uint32_t truth_index = address_parser_get_class_index(parser, truth);

                    result->confusion[predicted_index * num_classes + truth_index]++;

                    if (print_errors) {
                        printf("%s\t%s\t%d\t%s\n", predicted, truth, i, data_set->tokenized_str->str);
                    }
                }
                result->num_predictions++;

            })

        } else {
            log_error("Error in prediction\n");
            tokenized_string_destroy(data_set->tokenized_str);
            break;
        }

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

    cstring_array_destroy(token_labels);

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

    size_t position = 0;
    
    bool print_errors = false;

    ssize_t arg_iterations;

    char *filename = NULL;
    char *addres_parser_dir = NULL;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--print-errors")) {
            print_errors = true;
            continue;
        } else if (position == 0) {
            filename = arg;
            position++;
        } else if (position == 1) {
            address_parser_dir = arg;
            position++;
        }
    }

    if (!address_dictionary_module_setup(NULL)) {
        log_error("Could not load address dictionaries\n");
        exit(EXIT_FAILURE);
    }

    log_info("address dictionary module loaded\n");

    // Needs to load for normalization
    if (!transliteration_module_setup(NULL)) {
        log_error("Could not load transliteration module\n");
        exit(EXIT_FAILURE);
    }

    log_info("transliteration module loaded\n");

    if (!address_parser_load(address_parser_dir)) {
        log_error("Could not initialize parser\n");
        exit(EXIT_FAILURE);
    }

    log_info("Finished initialization\n");

    address_parser_t *parser = get_address_parser();

    if (parser->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        printf("averaged perceptron parser\n");
    } else if (parser->model_type == ADDRESS_PARSER_TYPE_CRF) {
        printf("crf parser\n");
    }

    address_parser_test_results_t results = EMPTY_ADDRESS_PARSER_TEST_RESULT;

    if (!address_parser_test(parser, filename, &results, print_errors)) {
        log_error("Error in training\n");
        exit(EXIT_FAILURE);
    }

    printf("Errors: %zu / %zu (%f%%)\n", results.num_errors, results.num_predictions, (double)results.num_errors / results.num_predictions);
    printf("Addresses: %zu / %zu (%f%%)\n\n", results.num_address_errors, results.num_address_predictions, (double)results.num_address_errors / results.num_address_predictions);


    printf("Confusion matrix:\n\n");
    uint32_t num_classes = address_parser_num_classes(parser);

    size_t *confusion_sorted = uint32_array_argsort(results.confusion, num_classes * num_classes);

    for (ssize_t k = num_classes * num_classes - 1; k >= 0; k--) {
        uint32_t idx = confusion_sorted[k];

        uint32_t i = idx / num_classes;
        uint32_t j = idx % num_classes;

        uint32_t class_errors = results.confusion[idx];

        if (i == j) continue;

        if (class_errors > 0) {
            cstring_array *classes = address_parser_class_strings(parser);
            char *predicted = cstring_array_get_string(classes, i);
            char *truth = cstring_array_get_string(classes, j);

            printf("(%s, %s): %d\n", predicted, truth, class_errors);
        }

    }

    free(results.confusion);
    free(confusion_sorted);

    address_parser_module_teardown();
    transliteration_module_teardown();
    address_dictionary_module_teardown();
}
