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

static inline bool is_postal_code(char *label) {
    return string_equals(label, ADDRESS_PARSER_LABEL_POSTAL_CODE);
}

static inline bool is_admin_component(char *label) {
    return (string_equals(label, ADDRESS_PARSER_LABEL_SUBURB) ||
           string_equals(label, ADDRESS_PARSER_LABEL_CITY_DISTRICT) ||
           string_equals(label, ADDRESS_PARSER_LABEL_CITY) ||
           string_equals(label, ADDRESS_PARSER_LABEL_STATE_DISTRICT) ||
           string_equals(label, ADDRESS_PARSER_LABEL_ISLAND) ||
           string_equals(label, ADDRESS_PARSER_LABEL_STATE) ||
           string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY_REGION) ||
           string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY) ||
           string_equals(label, ADDRESS_PARSER_LABEL_WORLD_REGION));
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

    khash_t(str_uint32) *phrase_counts = kh_init(str_uint32);
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

    uint32_t i;

    phrase_stats_t stats;
    khash_t(int_uint32) *class_counts;

    uint32_t vocab_size = 0;
    size_t examples = 0;

    const char *token;
    char *normalized;
    uint32_t count;

    char *key;
    int ret = 0;

    bool is_postal;

    char *label;
    char *prev_label;

    char_array *token_builder = char_array_new();
    char_array *postcode_token_builder = char_array_new();
    char_array *sub_token_builder = char_array_new();

    char_array *phrase_builder = char_array_new();

    cstring_array *phrases = cstring_array_new();
    cstring_array *phrase_labels = cstring_array_new();

    char *phrase;

    phrase_array *dictionary_phrases = phrase_array_new();

    token_array *sub_tokens = token_array_new();

    trie_t *phrase_counts_trie = NULL;

    tokenized_string_t *tokenized_str;
    token_array *tokens;

    while (address_parser_data_set_next(data_set)) {
        tokenized_str = data_set->tokenized_str;

        if (tokenized_str == NULL) {
            log_error("tokenized str is NULL\n");
            kh_destroy(str_uint32, vocab);
            return NULL;
        }

        char *language = char_array_get_string(data_set->language);
        if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
            language = NULL;
        }

        str = tokenized_str->str;
        tokens = tokenized_str->tokens;

        prev_label = NULL;

        size_t num_strings = cstring_array_num_strings(tokenized_str->strings);

        cstring_array_clear(phrases);
        cstring_array_clear(phrase_labels);

        cstring_array_foreach(tokenized_str->strings, i, token, {
            token_t t = tokens->a[i];

            label = cstring_array_get_string(data_set->labels, i);
            if (label == NULL) {
                continue;
            }

            char_array_clear(token_builder);

            bool is_admin = is_admin_component(label);
            is_postal = !is_admin && is_postal_code(label);

            uint64_t normalize_token_options = ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS;

            if (is_admin || is_postal) {
                normalize_token_options = ADDRESS_PARSER_NORMALIZE_ADMIN_TOKEN_OPTIONS;
            }

            add_normalized_token(token_builder, str, t, normalize_token_options);
            if (token_builder->n == 0) {
                continue;
            }

            normalized = char_array_get_string(token_builder);

            if (!is_admin && !is_postal) {
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

            if (is_postal) {
                char_array_clear(postcode_token_builder);
                add_normalized_token(postcode_token_builder, str, t, ADDRESS_PARSER_NORMALIZE_POSTAL_CODE_TOKEN_OPTIONS);
                char *postcode_normalized = char_array_get_string(postcode_token_builder);

                token_array_clear(sub_tokens);
                phrase_array_clear(dictionary_phrases);
                tokenize_add_tokens(sub_tokens, postcode_normalized, strlen(postcode_normalized), false);

                // One specific case where "CP" or "CEP" can be concatenated onto the front of the token
                if (sub_tokens->n > 1 && search_address_dictionaries_tokens_with_phrases(postcode_normalized, sub_tokens, language, &dictionary_phrases) && dictionary_phrases->n > 0) {
                    phrase_t first_postcode_phrase = dictionary_phrases->a[0];
                    address_expansion_value_t *value = address_dictionary_get_expansions(first_postcode_phrase.data);
                    if (value != NULL && value->components & ADDRESS_POSTAL_CODE) {
                        char_array_clear(token_builder);
                        size_t first_real_token_index = first_postcode_phrase.start + first_postcode_phrase.len;
                        token_t first_real_token =  sub_tokens->a[first_real_token_index];
                        char_array_cat(token_builder, postcode_normalized + first_real_token.offset);
                        normalized = char_array_get_string(token_builder);
                    }
                }
            }


            bool same_as_previous_label = string_equals(label, prev_label);

            if (prev_label == NULL || !same_as_previous_label || i == num_strings - 1) {
                if (i == num_strings - 1 && (same_as_previous_label || prev_label == NULL)) {
                    if (prev_label != NULL) {
                       char_array_cat(phrase_builder, " ");
                    }

                    char_array_cat(phrase_builder, normalized);
                }

                // End of phrase, add to hashtable
                if (prev_label != NULL) {
                    bool last_was_postal = string_equals(prev_label, ADDRESS_PARSER_LABEL_POSTAL_CODE);

                    phrase = char_array_get_string(phrase_builder);

                    if (last_was_postal) {
                        token_array_clear(sub_tokens);
                        phrase_array_clear(dictionary_phrases);

                        tokenize_add_tokens(sub_tokens, phrase, strlen(phrase), false);

                        if (sub_tokens->n > 0 && search_address_dictionaries_tokens_with_phrases(phrase, sub_tokens, language, &dictionary_phrases) && dictionary_phrases->n > 0) {
                            char_array_clear(sub_token_builder);

                            phrase_t current_phrase = NULL_PHRASE;
                            phrase_t prev_phrase = NULL_PHRASE;
                            token_t current_sub_token;

                            for (size_t pc = 0; pc < dictionary_phrases->n; pc++) {
                                current_phrase = dictionary_phrases->a[pc];
                                for (size_t j = prev_phrase.start + prev_phrase.len; j < current_phrase.start; j++) {
                                    current_sub_token = sub_tokens->a[j];

                                    char_array_cat_len(sub_token_builder, phrase + current_sub_token.offset, current_sub_token.len);

                                    if (j < current_phrase.start - 1) {
                                        char_array_cat(sub_token_builder, " ");
                                    }
                                }
                                prev_phrase = current_phrase;
                            }

                            if (prev_phrase.len > 0) {
                                for (size_t j = prev_phrase.start + prev_phrase.len; j < sub_tokens->n; j++) {
                                    current_sub_token = sub_tokens->a[j];

                                    char_array_cat_len(sub_token_builder, phrase + current_sub_token.offset, current_sub_token.len);

                                    if (j < sub_tokens->n - 1) {
                                        char_array_cat(sub_token_builder, " ");
                                    }
                                }
                            }

                            phrase = char_array_get_string(sub_token_builder);
                        }
                    }

                    log_info("phrase=%s\n", phrase);

                    cstring_array_add_string(phrases, phrase);
                    cstring_array_add_string(phrase_labels, prev_label);
                }

                if (i == num_strings - 1 && !same_as_previous_label && prev_label != NULL) {
                    log_info("phrase=%s\n", normalized);
                    cstring_array_add_string(phrases, normalized);
                    cstring_array_add_string(phrase_labels, label);
                }

                char_array_clear(phrase_builder);
            } else if (prev_label != NULL) {
                char_array_cat(phrase_builder, " ");
            }

            char_array_cat(phrase_builder, normalized);

            prev_label = label;

        })

        cstring_array_foreach(phrases, i, phrase, {
            if (phrase == NULL) continue;

            uint32_t class_id;
            uint32_t component = 0;

            char *phrase_label = cstring_array_get_string(phrase_labels, i);
            if (phrase_label == NULL) continue;

            is_postal = false;

            // Too many variations on these
            if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_CITY)) {
                class_id = ADDRESS_PARSER_BOUNDARY_CITY;
                component = ADDRESS_COMPONENT_CITY;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_STATE)) {
                class_id = ADDRESS_PARSER_BOUNDARY_STATE;
                component = ADDRESS_COMPONENT_STATE;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_COUNTRY)) {
                class_id = ADDRESS_PARSER_BOUNDARY_COUNTRY;
                component = ADDRESS_COMPONENT_COUNTRY;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_POSTAL_CODE)) {
                class_id = ADDRESS_PARSER_BOUNDARY_POSTAL_CODE;
                component = ADDRESS_COMPONENT_POSTAL_CODE;
                is_postal = true;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_COUNTRY_REGION)) {
                class_id = ADDRESS_PARSER_BOUNDARY_COUNTRY_REGION;
                component = ADDRESS_COMPONENT_COUNTRY_REGION;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_STATE_DISTRICT)) {
                class_id = ADDRESS_PARSER_BOUNDARY_STATE_DISTRICT;
                component = ADDRESS_COMPONENT_STATE_DISTRICT;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_SUBURB)) {
                class_id = ADDRESS_PARSER_BOUNDARY_SUBURB;
                component = ADDRESS_COMPONENT_SUBURB;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_CITY_DISTRICT)) {
                class_id = ADDRESS_PARSER_BOUNDARY_CITY_DISTRICT;
                component = ADDRESS_COMPONENT_CITY_DISTRICT;
            } else if (string_equals(label, ADDRESS_PARSER_LABEL_WORLD_REGION)) {
                class_id = ADDRESS_PARSER_BOUNDARY_WORLD_REGION;
                component = ADDRESS_COMPONENT_WORLD_REGION;
            } else if (string_equals(label, ADDRESS_PARSER_LABEL_ISLAND)) {
                class_id = ADDRESS_PARSER_BOUNDARY_ISLAND;
                component = ADDRESS_COMPONENT_ISLAND;
            } else {
                // Shouldn't happen but just in case
                continue;
            }

            char *normalized_phrase = NULL;

            if (string_contains_hyphen(phrase) && !is_postal) {
                normalized_phrase = normalize_string_utf8(phrase, NORMALIZE_STRING_REPLACE_HYPHENS);
            }

            char *phrases[2];
            phrases[0] = phrase;
            phrases[1] = normalized_phrase;

            for (int p_i = 0; p_i < sizeof(phrases) / sizeof(char *); p_i++) {
                phrase = phrases[p_i];
                if (phrase == NULL) continue;
                log_info("adding: %s\n", phrase);

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

                k = kh_get(str_uint32, phrase_counts, phrase);

                if (k != kh_end(phrase_counts)) {
                    kh_value(phrase_counts, k)++;
                } else {
                    key = strdup(phrase);
                    ret = 0;
                    k = kh_put(str_uint32, phrase_counts, key, &ret);
                    if (ret < 0) {
                        log_error("Error in kh_put in phrase_counts\n");
                        free(key);
                        if (normalized_phrase != NULL) {
                            free(normalized_phrase);
                        }
                        goto exit_hashes_allocated;
                    }
                    kh_value(phrase_counts, k) = 1;
                }
            }

            if (normalized_phrase != NULL) {
                log_info("freeing\n");
                free(normalized_phrase);
                normalized_phrase = NULL;
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

    phrase_counts_trie = trie_new_from_hash(phrase_counts);

    kh_foreach(phrase_stats, token, stats, {
        class_counts = stats.class_counts;
        int most_common = -1;
        uint32_t max_count = 0;
        uint32_t total = 0;
        for (int i = 0; i < NUM_ADDRESS_PARSER_BOUNDARY_TYPES; i++) {
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

    parser->model = NULL;

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
    char_array_destroy(postcode_token_builder);
    char_array_destroy(phrase_builder);
    cstring_array_destroy(phrases);
    cstring_array_destroy(phrase_labels);
    phrase_array_destroy(dictionary_phrases);
    token_array_destroy(sub_tokens);
    address_parser_data_set_destroy(data_set);

    if (phrase_counts_trie != NULL) {
        trie_destroy(phrase_counts_trie);
    }

    kh_foreach(vocab, token, count, {
        free((char *)token);
    })
    kh_destroy(str_uint32, vocab);

    kh_foreach(phrase_stats, token, stats, {
        kh_destroy(int_uint32, stats.class_counts);
        free((char *)token);
    })

    kh_destroy(phrase_stats, phrase_stats);

    kh_foreach(phrase_counts, token, count, {
        free((char *)token);
    })

    kh_destroy(str_uint32, phrase_counts);

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

    ssize_t arg_iterations;

    char *filename = NULL;
    char *output_dir = NULL;

    for (int i = pos_args; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--iterations")) {
            kwarg = ADDRESS_PARSER_TRAIN_ARG_ITERATIONS;
            continue;
        }

        if (kwarg == ADDRESS_PARSER_TRAIN_ARG_ITERATIONS) {
            if (sscanf(arg, "%zd", &arg_iterations) != 1 || arg_iterations < 0) {
                log_error("Bad arg for --iterations: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            num_iterations = (size_t)arg_iterations;
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
