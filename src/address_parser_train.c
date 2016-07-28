#include "address_parser.h"
#include "address_parser_io.h"
#include "address_dictionary.h"
#include "averaged_perceptron_trainer.h"
#include "collections.h"
#include "constants.h"
#include "file_utils.h"
#include "geodb.h"
#include "shuffle.h"
#include "transliterate.h"

#include "log/log.h"


typedef struct phrase_stats {
    khash_t(int_uint32) *class_counts;
    address_parser_types_t parser_types;
} phrase_stats_t;

KHASH_MAP_INIT_STR(phrase_stats, phrase_stats_t)

// Training

#define DEFAULT_ITERATIONS 5

#define MIN_VOCAB_COUNT 5
#define MIN_PHRASE_COUNT 1

static inline bool is_phrase_component(char *label) {
    return (string_equals(label, ADDRESS_PARSER_LABEL_SUBURB) ||
           string_equals(label, ADDRESS_PARSER_LABEL_CITY_DISTRICT) ||
           string_equals(label, ADDRESS_PARSER_LABEL_CITY) ||
           string_equals(label, ADDRESS_PARSER_LABEL_STATE_DISTRICT) ||
           string_equals(label, ADDRESS_PARSER_LABEL_ISLAND) ||
           string_equals(label, ADDRESS_PARSER_LABEL_STATE) ||
           string_equals(label, ADDRESS_PARSER_LABEL_POSTAL_CODE) ||
           string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY_REGION) ||
           string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY));
}

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

            char *label = cstring_array_get_string(data_set->labels, i);
            if (label == NULL) {
                continue;
            }

            char_array_clear(token_builder);

            bool is_phrase_label = is_phrase_component(label);

            uint64_t normalize_token_options = is_phrase_label ? ADDRESS_PARSER_NORMALIZE_PHRASE_TOKEN_OPTIONS : ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS;

            add_normalized_token(token_builder, str, t, normalize_token_options);
            if (token_builder->n == 0) {
                continue;
            }

            normalized = char_array_get_string(token_builder);

            if (!is_phrase_component(label)) {
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
                   if (string_equals(prev_label, ADDRESS_PARSER_LABEL_CITY)) {
                        class_id = ADDRESS_PARSER_CITY;
                        component = ADDRESS_COMPONENT_CITY;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_STATE)) {
                        class_id = ADDRESS_PARSER_STATE;
                        component = ADDRESS_COMPONENT_STATE;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_COUNTRY)) {
                        class_id = ADDRESS_PARSER_COUNTRY;
                        component = ADDRESS_COMPONENT_COUNTRY;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_COUNTRY_REGION)) {
                        class_id = ADDRESS_PARSER_COUNTRY_REGION;
                        component = ADDRESS_COMPONENT_COUNTRY;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_STATE_DISTRICT)) {
                        class_id = ADDRESS_PARSER_STATE_DISTRICT;
                        component = ADDRESS_COMPONENT_STATE_DISTRICT;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_SUBURB)) {
                        class_id = ADDRESS_PARSER_SUBURB;
                        component = ADDRESS_COMPONENT_SUBURB;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_CITY_DISTRICT)) {
                        class_id = ADDRESS_PARSER_CITY_DISTRICT;
                        component = ADDRESS_COMPONENT_CITY_DISTRICT;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_ISLAND)) {
                        class_id = ADDRESS_PARSER_ISLAND;
                        component = ADDRESS_COMPONENT_ISLAND;
                    } else if (string_equals(prev_label, ADDRESS_PARSER_LABEL_POSTAL_CODE)) {
                        class_id = ADDRESS_PARSER_POSTAL_CODE;
                        component = ADDRESS_COMPONENT_POSTAL_CODE;
                    } else {
                        // Shouldn't happen but just in case
                        prev_label = NULL;
                        continue;
                    }

                    char *phrase = char_array_get_string(phrase_builder);

                    char *normalized_phrase = NULL;

                    if (string_contains_hyphen(phrase)) {
                        char *phrase_copy = strdup(phrase);
                        if (phrase_copy == NULL) {
                            goto exit_hashes_allocated;
                        }
                        normalized_phrase = normalize_string_utf8(phrase_copy, NORMALIZE_STRING_REPLACE_HYPHENS);

                    }

                    char *phrases[2];
                    phrases[0] = phrase;
                    phrases[1] = normalized_phrase;

                    for (int i = 0; i < sizeof(phrases) / sizeof(char *); i++) {
                        phrase = phrases[i];
                        if (phrase == NULL) continue;

                        k = kh_get(phrase_stats, phrase_stats, phrase);

                        if (k == kh_end(phrase_stats)) {
                            key = strdup(phrase);
                            ret = 0;
                            k = kh_put(phrase_stats, phrase_stats, key, &ret);
                            if (ret < 0) {
                                log_error("Error in kh_put in phrase_stats\n");
                                free(key);
                                if (normalized_phrase != NULL) {
                                    free(normalized_phrase);
                                }
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

                    if (normalized_phrase != NULL) {
                        free(normalized_phrase);
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

typedef enum {
    ADDRESS_PARSER_TRAIN_POSITIONAL_ARG,
    ADDRESS_PARSER_TRAIN_ARG_ITERATIONS
} address_parser_train_keyword_arg_t;

#define USAGE "Usage: ./address_parser_train filename output_dir [--iterations number]\n"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    #if !defined(HAVE_SHUF)
    log_warn("shuf must be installed to train address parser effectively. If this is a production machine, please install shuf. No shuffling will be performed.\n");
    #endif

    int pos_args = 1;
    
    address_parser_train_keyword_arg_t kwarg = ADDRESS_PARSER_TRAIN_POSITIONAL_ARG;

    size_t num_iterations = DEFAULT_ITERATIONS;
    size_t position = 0;

    char *filename = NULL;
    char *output_dir = NULL;

    for (int i = pos_args; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--iterations")) {
            kwarg = ADDRESS_PARSER_TRAIN_ARG_ITERATIONS;
            continue;
        }

        if (kwarg == ADDRESS_PARSER_TRAIN_ARG_ITERATIONS) {
            num_iterations = (size_t)atoi(arg);
        } else if (position == 0) {
            filename = arg;
            position++;
        } else if (position == 1) {
            output_dir = arg;
            position++;
        }
        kwarg = ADDRESS_PARSER_TRAIN_POSITIONAL_ARG;

    }

    if (filename == NULL || output_dir == NULL) {
        printf(USAGE);
        exit(EXIT_FAILURE);
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

    if (!address_parser_train(parser, filename, num_iterations)) {
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
