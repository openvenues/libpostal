#include "address_parser.h"
#include "address_parser_io.h"
#include "address_dictionary.h"
#include "averaged_perceptron_trainer.h"
#include "collections.h"
#include "constants.h"
#include "file_utils.h"
#include "geodb.h"
#include "shuffle.h"

#include "log/log.h"

// Training

#define DEFAULT_ITERATIONS 5

#define MIN_VOCAB_COUNT 5

address_parser_t *address_parser_init(char *filename) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return NULL;
    }

    address_parser_data_set_t *data_set = address_parser_data_set_init(filename);

    if (data_set == NULL) {
        log_error("Error initializing data set\n");
        return NULL;
    }


    address_parser_t *parser = address_parser_new();
    if (parser == NULL) {
        log_error("Error allocating parser\n");
        return NULL;
    }

    khash_t(str_uint32) *vocab = kh_init(str_uint32);

    khiter_t k;
    char *str;

    uint32_t vocab_size = 0;
    size_t examples = 0;

    const char *word;

    uint32_t i;
    char *token;
    char *normalized;
    uint32_t count;

    char_array *token_array = char_array_new();

    while (address_parser_data_set_next(data_set)) {
        tokenized_string_t *tokenized_str = data_set->tokenized_str;

        if (tokenized_str == NULL) {
            log_error("tokenized str is NULL\n");
            kh_destroy(str_uint32, vocab);
            return false;
        }

        str = tokenized_str->str;

        cstring_array_foreach(tokenized_str->strings, i, token, {
            token_t t = tokenized_str->tokens->a[i];

            char_array_clear(token_array);
            add_normalized_token(token_array, str, t, ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS);
            if (token_array->n == 0) {
                continue;
            }

            normalized = char_array_get_string(token_array);


            k = kh_get(str_uint32, vocab, normalized);
            if (k == kh_end(vocab)) {
                int ret;
                char *key = strdup(normalized);
                k = kh_put(str_uint32, vocab, key, &ret);
                if (ret < 0) {
                    log_error("Error in kh_put\n");
                    free(key);
                    tokenized_string_destroy(tokenized_str);
                    kh_foreach(vocab, word, count, {
                        free((char *)word);
                    })
                    kh_destroy(str_uint32, vocab);
                    char_array_destroy(token_array);
                    return false;
                }
                kh_value(vocab, k) = 1;
                vocab_size++;
            } else {
                kh_value(vocab, k)++;
            }

        })

        tokenized_string_destroy(tokenized_str);
        examples++;
        if (examples % 10000 == 0 && examples != 0) {
            log_info("Counting vocab: did %zu examples\n", examples);
        }
    }

    log_debug("Done with vocab, total size=%d\n", vocab_size);

    for (k = kh_begin(vocab); k != kh_end(vocab); ++k) {
        char *word = (char *)kh_key(vocab, k);
        if (!kh_exist(vocab, k)) {
            continue;
        }
        uint32_t count = kh_value(vocab, k);
        if (count < MIN_VOCAB_COUNT) {
            kh_del(str_uint32, vocab, k);
            free(word);
        }
    }

    parser->vocab = trie_new_from_hash(vocab);

    for (k = kh_begin(vocab); k != kh_end(vocab); ++k) {
        if (!kh_exist(vocab, k)) {
            continue;
        }
        char *word = (char *)kh_key(vocab, k);
        free(word);
    }

    kh_destroy(str_uint32, vocab);

    char_array_destroy(token_array);
    address_parser_data_set_destroy(data_set);
    if (parser->vocab == NULL) {
        log_error("Error initializing vocabulary\n");
        address_parser_destroy(parser);
        return NULL;
    }

    return parser;
}



bool address_parser_train_epoch(address_parser_t *self, averaged_perceptron_trainer_t *trainer, char *filename) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return false;
    }

    address_parser_data_set_t *data_set = address_parser_data_set_init(filename);
    if (data_set == NULL) {
        log_error("Error initializing data set\n");
        return false;
    }

    address_parser_context_t *context = address_parser_context_new();

    bool success = false;

    size_t examples = 0;
    size_t errors = trainer->num_errors;

    bool logged = false;

    while (address_parser_data_set_next(data_set)) {
        char *language = char_array_get_string(data_set->language);
        if (strcmp(language, UNKNOWN_LANGUAGE) == 0 || strcmp(language, AMBIGUOUS_LANGUAGE) == 0) {
            language = NULL;
        }
        char *country = char_array_get_string(data_set->country);

        address_parser_context_fill(context, data_set->tokenized_str, language, country);

        bool example_success = averaged_perceptron_trainer_train_example(trainer, self, context, context->features, &address_parser_features, data_set->tokenized_str, data_set->labels);

        if (!example_success) {
            log_error("Error training example\n");
            goto exit_epoch_training_started;
        }

        tokenized_string_destroy(data_set->tokenized_str);
        data_set->tokenized_str = NULL;

        if (!example_success) {
            log_error("Error training example without country/language\n");
            goto exit_epoch_training_started;
        }

        examples++;
        if (examples % 1000 == 0 && examples > 0) {
            log_info("Iter %d: Did %zu examples with %llu errors\n", trainer->iterations, examples, trainer->num_errors - errors);
            errors = trainer->num_errors;
        }

    }

    success = true;

exit_epoch_training_started:
    address_parser_data_set_destroy(data_set);
    address_parser_context_destroy(context);

    return success;
}

bool address_parser_train(address_parser_t *self, char *filename, uint32_t num_iterations) {
    averaged_perceptron_trainer_t *trainer = averaged_perceptron_trainer_new();

    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        log_info("Doing epoch %d\n", iter);

        trainer->iterations = iter;

        log_debug("Shuffling\n");

        /*
        if (!shuffle_file(filename)) {
            log_error("Error in shuffle\n");
            averaged_perceptron_trainer_destroy(trainer);
            return false;
        }

        log_debug("Shuffle complete\n");
        */
        if (!address_parser_train_epoch(self, trainer, filename)) {
            log_error("Error in epoch\n");
            averaged_perceptron_trainer_destroy(trainer);
            return false;
        }
    }

    log_debug("Done with training, averaging weights\n");

    self->model = averaged_perceptron_trainer_finalize(trainer);
    

    return true;
}


int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: ./address_parser_train filename output_dir\n");
        exit(EXIT_FAILURE);
    }

    #if !defined(HAVE_SHUF) && !defined(HAVE_GSHUF)
    log_error("shuf or gshuf must be installed to train address parser. Please install and reconfigure libpostal\n");
    exit(EXIT_FAILURE);
    #endif

    char *filename = argv[1];
    char *output_dir = argv[2];

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

    address_parser_t *parser = address_parser_init(filename);

    if (parser == NULL) {
        log_error("Could not initialize parser\n");
        exit(EXIT_FAILURE);
    }

    log_info("Finished initialization\n");

    if (!address_parser_train(parser, filename, DEFAULT_ITERATIONS)) {
        log_error("Error in training\n");
        exit(EXIT_FAILURE);
    }

    log_debug("Finished training\n");

    if (!address_parser_save(parser, output_dir)) {
        log_error("Error saving address parser\n");
        exit(EXIT_FAILURE);
    }

    address_parser_destroy(parser);

    address_dictionary_module_teardown();
    geodb_module_teardown();
    log_debug("Done\n");
}
