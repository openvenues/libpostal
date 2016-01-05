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
#define MIN_PHRASE_COUNT 1

typedef struct phrase_stats {
    khash_t(int_uint32) *class_counts;
    address_parser_types_t parser_types;
} phrase_stats_t;

KHASH_MAP_INIT_STR(phrase_stats, phrase_stats_t)

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
    if (vocab == NULL) {
        log_error("Could not allocate vocab\n");
        return NULL;
    }

    khash_t(phrase_stats) *phrase_stats = kh_init(phrase_stats);
    if (phrase_stats == NULL) {
        log_error("Could not allocate phrase_stats\n");
        return NULL;
    }

    khash_t(str_uint32) *phrase_types = kh_init(str_uint32);
    if (phrase_types == NULL) {
        log_error("Could not allocate phrase_types\n");
        return NULL;
    }

    khiter_t k;
    char *str;

    phrase_stats_t stats;
    khash_t(int_uint32) *class_counts;

    uint32_t vocab_size = 0;
    size_t examples = 0;

    uint32_t i;
    const char *token;
    char *normalized;
    uint32_t count;

    char *key;
    int ret = 0;

    char_array *token_builder = char_array_new();

    char_array *phrase_builder = char_array_new();

    while (address_parser_data_set_next(data_set)) {
        tokenized_string_t *tokenized_str = data_set->tokenized_str;

        if (tokenized_str == NULL) {
            log_error("tokenized str is NULL\n");
            kh_destroy(str_uint32, vocab);
            return NULL;
        }

        str = tokenized_str->str;

        char *prev_label = NULL;

        size_t num_strings = cstring_array_num_strings(tokenized_str->strings);

        cstring_array_foreach(tokenized_str->strings, i, token, {
            token_t t = tokenized_str->tokens->a[i];

            char_array_clear(token_builder);
            add_normalized_token(token_builder, str, t, ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS);
            if (token_builder->n == 0) {
                continue;
            }

            normalized = char_array_get_string(token_builder);

            k = kh_get(str_uint32, vocab, normalized);
            
            if (k == kh_end(vocab)) {
                key = strdup(normalized);
                k = kh_put(str_uint32, vocab, key, &ret);
                if (ret < 0) {
                    log_error("Error in kh_put in vocab\n");
                    free(key);
                    goto exit_hashes_allocated;
                }
                kh_value(vocab, k) = 1;
                vocab_size++;
            } else {
                kh_value(vocab, k)++;
            }

            char *label = cstring_array_get_string(data_set->labels, i);
            if (label == NULL) {
                continue;
            }

            if (string_equals(label, "road") || string_equals(label, "house_number") || string_equals(label, "house")) {
                prev_label = NULL;
                continue;
            }

            if (prev_label == NULL || !string_equals(label, prev_label) || i == num_strings - 1) {

                if (i == num_strings - 1) {
                    if (!string_equals(label, prev_label)) {
                        char_array_clear(phrase_builder);
                    } else if (prev_label != NULL) {
                       char_array_cat(phrase_builder, " ");
                    }

                    char_array_cat(phrase_builder, normalized);
                    prev_label = label;
                }

                // End of phrase, add to hashtable
                if (prev_label != NULL) {
                    uint32_t class_id;
                    uint32_t component = 0;

                    // Too many variations on these
                   if (string_equals(prev_label, "city")) {
                        class_id = ADDRESS_PARSER_CITY;
                        component = ADDRESS_COMPONENT_CITY;
                    } else if (string_equals(prev_label, "state")) {
                        class_id = ADDRESS_PARSER_STATE;
                        component = ADDRESS_COMPONENT_STATE;
                    } else if (string_equals(prev_label, "country")) {
                        class_id = ADDRESS_PARSER_COUNTRY;
                        component = ADDRESS_COMPONENT_COUNTRY;
                    } else if (string_equals(prev_label, "state_district")) {
                        class_id = ADDRESS_PARSER_STATE_DISTRICT;
                        component = ADDRESS_COMPONENT_STATE_DISTRICT;
                    } else if (string_equals(prev_label, "suburb")) {
                        class_id = ADDRESS_PARSER_SUBURB;
                        component = ADDRESS_COMPONENT_SUBURB;
                    } else if (string_equals(prev_label, "city_district")) {
                        class_id = ADDRESS_PARSER_CITY_DISTRICT;
                        component = ADDRESS_COMPONENT_CITY_DISTRICT;
                    } else if (string_equals(prev_label, "postcode")) {
                        class_id = ADDRESS_PARSER_POSTAL_CODE;
                        component = ADDRESS_COMPONENT_POSTAL_CODE;
                    }

                    char *phrase = char_array_get_string(phrase_builder);

                    k = kh_get(phrase_stats, phrase_stats, phrase);

                    if (k == kh_end(phrase_stats)) {
                        key = strdup(phrase);
                        ret = 0;
                        k = kh_put(phrase_stats, phrase_stats, key, &ret);
                        if (ret < 0) {
                            log_error("Error in kh_put in phrase_stats\n");
                            free(key);
                            goto exit_hashes_allocated;
                        }
                        class_counts = kh_init(int_uint32);
                    
                        stats.class_counts = class_counts;
                        stats.parser_types.components = component;
                        stats.parser_types.most_common = 0;
                        
                        kh_value(phrase_stats, k) = stats;
                    } else {
                        stats = kh_value(phrase_stats, k);
                        class_counts = stats.class_counts;
                        stats.parser_types.components |= component;

                    }

                    k = kh_get(int_uint32, class_counts, (khint_t)class_id);

                    if (k == kh_end(class_counts)) {
                        ret = 0;
                        k = kh_put(int_uint32, class_counts, class_id, &ret);
                        if (ret < 0) {
                            log_error("Error in kh_put in class_counts\n");
                            goto exit_hashes_allocated;
                        }
                        kh_value(class_counts, k) = 1;
                    } else {
                        kh_value(class_counts, k)++;
                    }

                }

                char_array_clear(phrase_builder);
            } else if (prev_label != NULL) {
                char_array_cat(phrase_builder, " ");
            }

            char_array_cat(phrase_builder, normalized);


            prev_label = label;

        })

        tokenized_string_destroy(tokenized_str);
        examples++;
        if (examples % 10000 == 0 && examples != 0) {
            log_info("Counting vocab: did %zu examples\n", examples);
        }
    }

    log_debug("Done with vocab, total size=%d\n", vocab_size);

    for (k = kh_begin(vocab); k != kh_end(vocab); ++k) {
        token = (char *)kh_key(vocab, k);
        if (!kh_exist(vocab, k)) {
            continue;
        }
        uint32_t count = kh_value(vocab, k);
        if (count < MIN_VOCAB_COUNT) {
            kh_del(str_uint32, vocab, k);
            free((char *)token);
        }
    }

    kh_foreach(phrase_stats, token, stats, {
        class_counts = stats.class_counts;
        int most_common = -1;
        uint32_t max_count = 0;
        uint32_t total = 0;
        for (int i = 0; i < NUM_ADDRESS_PARSER_TYPES; i++) {
            k = kh_get(int_uint32, class_counts, (khint_t)i);
            if (k != kh_end(class_counts)) {
                count = kh_value(class_counts, k);

                if (count > max_count) {
                    max_count = count;
                    most_common = i;
                }
                total += count;
            }
        }

        if (most_common > -1 && total >= MIN_PHRASE_COUNT) {
            stats.parser_types.most_common = (uint32_t)most_common;
            ret = 0;
            char *key = strdup(token);
            k = kh_put(str_uint32, phrase_types, key, &ret);
            if (ret < 0) {
                log_error("Error on kh_put in phrase_types\n");
                free(key);
                goto exit_hashes_allocated;
            }

            kh_value(phrase_types, k) = stats.parser_types.value;
        }
    })

    parser->vocab = trie_new_from_hash(vocab);
    if (parser->vocab == NULL) {
        log_error("Error initializing vocabulary\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    parser->phrase_types = trie_new_from_hash(phrase_types);
    if (parser->phrase_types == NULL) {
        log_error("Error converting phrase_types to trie\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

exit_hashes_allocated:
    // Free memory for hashtables, etc.

    char_array_destroy(token_builder);
    char_array_destroy(phrase_builder);
    address_parser_data_set_destroy(data_set);

    kh_foreach(vocab, token, count, {
        free((char *)token);
    })
    kh_destroy(str_uint32, vocab);

    kh_foreach(phrase_stats, token, stats, {
        kh_destroy(int_uint32, stats.class_counts);
        free((char *)token);
    })

    kh_destroy(phrase_stats, phrase_stats);

    kh_foreach(phrase_types, token, count, {
        free((char *)token);
    })
    kh_destroy(str_uint32, phrase_types);

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
        if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
            language = NULL;
        }
        char *country = char_array_get_string(data_set->country);

        address_parser_context_fill(context, self, data_set->tokenized_str, language, country);

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

        #if defined(HAVE_SHUF)
        log_info("Shuffling\n");

        if (!shuffle_file(filename)) {
            log_error("Error in shuffle\n");
            averaged_perceptron_trainer_destroy(trainer);
            return false;
        }

        log_info("Shuffle complete\n");
        #endif
        
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

    #if !defined(HAVE_SHUF)
    log_warn("shuf must be installed to train address parser effectively. If this is a production machine, please install shuf. No shuffling will be performed.\n");
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
