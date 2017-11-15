#include <stdint.h>

#include "address_parser.h"
#include "address_parser_io.h"
#include "address_dictionary.h"
#include "averaged_perceptron_trainer.h"
#include "crf_trainer_averaged_perceptron.h"
#include "collections.h"
#include "constants.h"
#include "file_utils.h"
#include "graph.h"
#include "graph_builder.h"
#include "shuffle.h"
#include "transliterate.h"

#include "log/log.h"

typedef struct phrase_stats {
    khash_t(int_uint32) *class_counts;
    uint16_t components;
} phrase_stats_t;

KHASH_MAP_INIT_STR(phrase_stats, phrase_stats_t)
KHASH_MAP_INIT_STR(postal_code_context_phrases, khash_t(str_set) *)
KHASH_MAP_INIT_STR(phrase_types, address_parser_types_t)

// Training

#define DEFAULT_ITERATIONS 5
#define DEFAULT_MIN_UPDATES 5
#define DEFAULT_MODEL_TYPE ADDRESS_PARSER_TYPE_CRF

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

typedef struct vocab_context {
    char_array *token_builder;
    char_array *postal_code_token_builder;
    char_array *sub_token_builder;
    char_array *phrase_builder;
    phrase_array *dictionary_phrases;
    int64_array *phrase_memberships;
    phrase_array *postal_code_dictionary_phrases;
    token_array *sub_tokens;
} vocab_context_t;

bool address_phrases_and_labels(address_parser_data_set_t *data_set, cstring_array *phrases, cstring_array *phrase_labels, vocab_context_t *ctx) {
    tokenized_string_t *tokenized_str = data_set->tokenized_str;
    if (tokenized_str == NULL) {
        log_error("tokenized_str == NULL\n");
        return false;
    }

    char *language = char_array_get_string(data_set->language);
    if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
        language = NULL;
    }

    char_array *token_builder = ctx->token_builder;
    char_array *postal_code_token_builder = ctx->postal_code_token_builder;
    char_array *sub_token_builder = ctx->sub_token_builder;
    char_array *phrase_builder = ctx->phrase_builder;
    phrase_array *dictionary_phrases = ctx->dictionary_phrases;
    int64_array *phrase_memberships = ctx->phrase_memberships;
    phrase_array *postal_code_dictionary_phrases = ctx->postal_code_dictionary_phrases;
    token_array *sub_tokens = ctx->sub_tokens;

    uint32_t i = 0;
    uint32_t j = 0;

    char *normalized;
    char *phrase;

    char *label;
    char *prev_label;

    const char *token;

    char *str = tokenized_str->str;
    token_array *tokens = tokenized_str->tokens;

    prev_label = NULL;

    size_t num_strings = cstring_array_num_strings(tokenized_str->strings);

    cstring_array_clear(phrases);
    cstring_array_clear(phrase_labels);

    bool is_admin = false;
    bool is_postal = false;
    bool have_postal_code = false;

    bool last_was_separator = false;

    int64_array_clear(phrase_memberships);
    phrase_array_clear(dictionary_phrases);
    char_array_clear(postal_code_token_builder);

    // One specific case where "CP" or "CEP" can be concatenated onto the front of the token
    bool have_dictionary_phrases = search_address_dictionaries_tokens_with_phrases(tokenized_str->str, tokenized_str->tokens, language, &dictionary_phrases);
    token_phrase_memberships(dictionary_phrases, phrase_memberships, tokenized_str->tokens->n);

    cstring_array_foreach(tokenized_str->strings, i, token, {
        token_t t = tokens->a[i];

        label = cstring_array_get_string(data_set->labels, i);
        if (label == NULL) {
            continue;
        }

        char_array_clear(token_builder);

        is_admin = is_admin_component(label);
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

        int64_t phrase_membership = NULL_PHRASE_MEMBERSHIP;

        if (!is_admin && !is_postal) {
            // Check if this is a (potentially multi-word) dictionary phrase
            phrase_membership = phrase_memberships->a[i];
            if (phrase_membership != NULL_PHRASE_MEMBERSHIP) {
                phrase_t current_phrase = dictionary_phrases->a[phrase_membership];

                if (current_phrase.start == i) {
                    char_array_clear(phrase_builder);
                    char *first_label = label;
                    bool invalid_phrase = false;
                    // On the start of every phrase, check that all its tokens have the
                    // same label, otherwise set to memberships to the null phrase
                    for (j = current_phrase.start + 1; j < current_phrase.start + current_phrase.len; j++) {
                        char *token_label = cstring_array_get_string(data_set->labels, j);
                        if (!string_equals(token_label, first_label)) {
                            for (j = current_phrase.start; j < current_phrase.start + current_phrase.len; j++) {
                                phrase_memberships->a[j] = NULL_PHRASE_MEMBERSHIP;
                            }
                            invalid_phrase = true;
                            break;
                        }
                    }
                    // If the phrase was invalid, add the single word
                    if (invalid_phrase) {
                        cstring_array_add_string(phrases, normalized);
                        cstring_array_add_string(phrase_labels, label);
                     }
                }
                // If we're in a valid phrase, add the current word to the phrase
                char_array_cat(phrase_builder, normalized);
                if (i < current_phrase.start + current_phrase.len - 1) {
                    char_array_cat(phrase_builder, " ");
                } else {
                    // If we're at the end of a phrase, add entire phrase as a string
                    normalized = char_array_get_string(phrase_builder);
                    cstring_array_add_string(phrases, normalized);
                    cstring_array_add_string(phrase_labels, label);
                }

            } else {

                cstring_array_add_string(phrases, normalized);
                cstring_array_add_string(phrase_labels, label);
            }

            prev_label = NULL;

            continue;
        }

        if (is_postal) {
            add_normalized_token(postal_code_token_builder, str, t, ADDRESS_PARSER_NORMALIZE_POSTAL_CODE_TOKEN_OPTIONS);
            char *postal_code_normalized = char_array_get_string(postal_code_token_builder);

            token_array_clear(sub_tokens);
            phrase_array_clear(postal_code_dictionary_phrases);
            tokenize_add_tokens(sub_tokens, postal_code_normalized, strlen(postal_code_normalized), false);

            // One specific case where "CP" or "CEP" can be concatenated onto the front of the token
            if (sub_tokens->n > 1 && search_address_dictionaries_tokens_with_phrases(postal_code_normalized, sub_tokens, language, &postal_code_dictionary_phrases) && postal_code_dictionary_phrases->n > 0) {
                phrase_t first_postal_code_phrase = postal_code_dictionary_phrases->a[0];
                address_expansion_value_t *value = address_dictionary_get_expansions(first_postal_code_phrase.data);
                if (value != NULL && value->components & LIBPOSTAL_ADDRESS_POSTAL_CODE) {
                    char_array_clear(token_builder);
                    size_t first_real_token_index = first_postal_code_phrase.start + first_postal_code_phrase.len;
                    token_t first_real_token =  sub_tokens->a[first_real_token_index];
                    char_array_cat(token_builder, postal_code_normalized + first_real_token.offset);
                    normalized = char_array_get_string(token_builder);
                }
            }
        }

        bool last_was_postal = string_equals(prev_label, ADDRESS_PARSER_LABEL_POSTAL_CODE);
        bool same_as_previous_label = string_equals(label, prev_label) && (!last_was_separator || last_was_postal);

        if (prev_label == NULL || !same_as_previous_label || i == num_strings - 1) {
            if (i == num_strings - 1 && (same_as_previous_label || prev_label == NULL)) {
                if (prev_label != NULL) {
                   char_array_cat(phrase_builder, " ");
                }

                char_array_cat(phrase_builder, normalized);
            }

            // End of phrase, add to hashtable
            if (prev_label != NULL) {

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

                            address_expansion_value_t *phrase_value = address_dictionary_get_expansions(current_phrase.data);
                            size_t current_phrase_end = current_phrase.start + current_phrase.len;
                            if (phrase_value != NULL && phrase_value->components & LIBPOSTAL_ADDRESS_POSTAL_CODE) {
                                current_phrase_end = current_phrase.start;
                            }

                            for (size_t j = prev_phrase.start + prev_phrase.len; j < current_phrase_end; j++) {
                                current_sub_token = sub_tokens->a[j];

                                char_array_cat_len(sub_token_builder, phrase + current_sub_token.offset, current_sub_token.len);

                                if (j < sub_tokens->n - 1) {
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

                cstring_array_add_string(phrases, phrase);
                cstring_array_add_string(phrase_labels, prev_label);

            }

            if (i == num_strings - 1 && !same_as_previous_label && prev_label != NULL) {
                cstring_array_add_string(phrases, normalized);
                cstring_array_add_string(phrase_labels, label);
            }

            char_array_clear(phrase_builder);
        } else if (prev_label != NULL) {
            char_array_cat(phrase_builder, " ");
        }

        char_array_cat(phrase_builder, normalized);

        prev_label = label;

        last_was_separator = data_set->separators->a[i] == ADDRESS_SEPARATOR_FIELD_INTERNAL;

    })

    return true;
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

    address_parser_context_t *context = address_parser_context_new();
    if (context == NULL) {
        log_error("Error allocating context\n");
        return NULL;
    }
    parser->context = context;

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

    khash_t(str_uint32) *class_counts = kh_init(str_uint32);
    if (class_counts == NULL) {
        log_error("Could not allocate class_counts\n");
        return NULL;
    }

    khash_t(phrase_stats) *phrase_stats = kh_init(phrase_stats);
    if (phrase_stats == NULL) {
        log_error("Could not allocate phrase_stats\n");
        return NULL;
    }

    khash_t(phrase_types) *phrase_types = kh_init(phrase_types);
    if (phrase_types == NULL) {
        log_error("Could not allocate phrase_types\n");
        return NULL;
    }

    khash_t(str_uint32) *postal_code_counts = kh_init(str_uint32);
    if (postal_code_counts == NULL) {
        log_error("Could not allocate postal_code_counts\n");
        return NULL;
    }

    khash_t(postal_code_context_phrases) *postal_code_admin_contexts = kh_init(postal_code_context_phrases);
    if (postal_code_admin_contexts == NULL) {
        log_error("Could not allocate postal_code_admin_contexts\n");
        return NULL;
    }

    khiter_t k;
    char *str;

    uint32_t i, j;

    phrase_stats_t stats;
    khash_t(int_uint32) *place_class_counts;

    size_t examples = 0;

    const char *token;
    char *normalized;
    uint32_t count;

    char *key;
    int ret = 0;

    postal_code_context_value_t pc_ctx;

    bool is_postal = false;

    char *label;
    char *prev_label;

    vocab_context_t *vocab_context = malloc(sizeof(vocab_context_t));
    if (vocab_context == NULL) {
        log_error("Error allocationg vocab_context\n");
        return NULL;
    }

    vocab_context->token_builder = char_array_new();
    vocab_context->postal_code_token_builder = char_array_new();
    vocab_context->sub_token_builder = char_array_new();
    vocab_context->phrase_builder = char_array_new();
    vocab_context->dictionary_phrases = phrase_array_new();
    vocab_context->phrase_memberships = int64_array_new();
    vocab_context->postal_code_dictionary_phrases = phrase_array_new();
    vocab_context->sub_tokens = token_array_new();

    if (vocab_context->token_builder == NULL ||
        vocab_context->postal_code_token_builder == NULL ||
        vocab_context->sub_token_builder == NULL ||
        vocab_context->phrase_builder == NULL ||
        vocab_context->dictionary_phrases == NULL ||
        vocab_context->phrase_memberships == NULL ||
        vocab_context->postal_code_dictionary_phrases == NULL ||
        vocab_context->sub_tokens == NULL) {
        log_error("Error initializing vocab_context\n");
        return NULL;
    }

    cstring_array *phrases = cstring_array_new();
    cstring_array *phrase_labels = cstring_array_new();

    if (phrases == NULL || phrase_labels == NULL) {
        log_error("Error setting up arrays for vocab building\n");
        return NULL;
    }

    char *phrase;

    trie_t *phrase_counts_trie = NULL;

    tokenized_string_t *tokenized_str;
    token_array *tokens;

    while (address_parser_data_set_next(data_set)) {
        tokenized_str = data_set->tokenized_str;

        if (tokenized_str == NULL) {
            log_error("tokenized str is NULL\n");
            goto exit_hashes_allocated;
        }

        if (!address_phrases_and_labels(data_set, phrases, phrase_labels, vocab_context)) {
            log_error("Error in address phrases and labels\n");
            goto exit_hashes_allocated;
        }

        // Iterate through one time to see if there is a postal code in the string
        bool have_postal_code = false;
        char *postal_code_phrase = NULL;

        cstring_array_foreach(phrases, i, phrase, {
            if (phrase == NULL) continue;
            char *phrase_label = cstring_array_get_string(phrase_labels, i);

            if (is_postal_code(phrase_label)) {
                have_postal_code = true;
                postal_code_phrase = phrase;
                break;
            }
        })

        cstring_array_foreach(phrase_labels, i, label, {
            if (!str_uint32_hash_incr(class_counts, label)) {
                log_error("Error in hash_incr for class_counts\n");
                goto exit_hashes_allocated;
            }
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
                is_postal = true;

                char_array *token_builder = vocab_context->token_builder;
                token_array *sub_tokens = vocab_context->sub_tokens;
                tokenize_add_tokens(sub_tokens, phrase, strlen(phrase), false);

                char_array_clear(token_builder);

                for (j = 0; j < sub_tokens->n; j++) {
                    token_array_clear(sub_tokens);
                    token_t t = sub_tokens->a[j];
                    add_normalized_token(token_builder, phrase, t, ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS);

                    if (token_builder->n == 0) {
                        continue;
                    }

                    char *sub_token = char_array_get_string(token_builder);
                    if (!str_uint32_hash_incr(vocab, sub_token)) {
                        log_error("Error in str_uint32_hash_incr\n");
                        goto exit_hashes_allocated;
                    }

                }

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
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_WORLD_REGION)) {
                class_id = ADDRESS_PARSER_BOUNDARY_WORLD_REGION;
                component = ADDRESS_COMPONENT_WORLD_REGION;
            } else if (string_equals(phrase_label, ADDRESS_PARSER_LABEL_ISLAND)) {
                class_id = ADDRESS_PARSER_BOUNDARY_ISLAND;
                component = ADDRESS_COMPONENT_ISLAND;
            } else {
                bool in_vocab = false;
                if (!str_uint32_hash_incr_exists(vocab, phrase, &in_vocab)) {
                    log_error("Error in str_uint32_hash_incr\n");
                    goto exit_hashes_allocated;
                }
                continue;
            }

            char *normalized_phrase = NULL;

            if (!is_postal && string_contains_hyphen(phrase)) {
                normalized_phrase = normalize_string_utf8(phrase, NORMALIZE_STRING_REPLACE_HYPHENS);
            }

            char *phrases[2];
            phrases[0] = phrase;
            phrases[1] = normalized_phrase;

            for (size_t p_i = 0; p_i < sizeof(phrases) / sizeof(char *); p_i++) {
                phrase = phrases[p_i];
                if (phrase == NULL) continue;

                if (is_postal) {
                    if (!str_uint32_hash_incr(postal_code_counts, phrase)) {
                        log_error("Error in str_uint32_hash_incr for postal_code_counts\n");
                        goto exit_hashes_allocated;
                    }
                    continue;
                }

                if (have_postal_code && !is_postal) {
                    khash_t(str_set) *context_postal_codes = NULL;

                    k = kh_get(postal_code_context_phrases, postal_code_admin_contexts, postal_code_phrase);
                    if (k == kh_end(postal_code_admin_contexts)) {
                        key = strdup(postal_code_phrase);
                        ret = 0;
                        k = kh_put(postal_code_context_phrases, postal_code_admin_contexts, key, &ret);

                        if (ret < 0) {
                            log_error("Error in kh_put in postal_code_admin_contexts\n");
                            free(key);
                            goto exit_hashes_allocated;
                        }
                        context_postal_codes = kh_init(str_set);
                        if (context_postal_codes == NULL) {
                            log_error("Error in kh_init for context_postal_codes\n");
                            free(key);
                            goto exit_hashes_allocated;
                        }
                        kh_value(postal_code_admin_contexts, k) = context_postal_codes;
                    } else {
                        context_postal_codes = kh_value(postal_code_admin_contexts, k);
                    }

                    k = kh_get(str_set, context_postal_codes, phrase);
                    if (k == kh_end(context_postal_codes)) {
                        char *context_key = strdup(phrase);
                        k = kh_put(str_set, context_postal_codes, context_key, &ret);
                        if (ret < 0) {
                            log_error("Error in kh_put in context_postal_codes\n");
                            free(context_key);
                            goto exit_hashes_allocated;
                        }
                    }
                }

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
                    place_class_counts = kh_init(int_uint32);
                
                    stats.class_counts = place_class_counts;
                    stats.components = component;
                    
                    kh_value(phrase_stats, k) = stats;
                } else {
                    stats = kh_value(phrase_stats, k);
                    place_class_counts = stats.class_counts;
                    stats.components |= component;
                    kh_value(phrase_stats, k) = stats;
                }

                if (!int_uint32_hash_incr(place_class_counts, (khint_t)class_id)) {
                    log_error("Error in int_uint32_hash_incr in class_counts\n");
                    goto exit_hashes_allocated;
                }

                if (!str_uint32_hash_incr(phrase_counts, phrase)) {
                    log_error("Error in str_uint32_hash_incr in phrase_counts\n");
                    goto exit_hashes_allocated;
                }

            }

            if (normalized_phrase != NULL) {
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

    log_info("Done with vocab, total size=%" PRIkh32 "\n", kh_size(vocab));

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

    log_info("After pruning vocab size=%" PRIkh32 "\n", kh_size(vocab));


    log_info("Creating phrases trie\n");


    phrase_counts_trie = trie_new_from_hash(phrase_counts);

    log_info("Calculating phrase types\n");

    size_t num_classes = kh_size(class_counts);
    log_info("num_classes = %zu\n", num_classes);
    parser->num_classes = num_classes;

    log_info("Creating vocab trie\n");

    parser->vocab = trie_new_from_hash(vocab);
    if (parser->vocab == NULL) {
        log_error("Error initializing vocabulary\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    kh_foreach(phrase_counts, token, count, {
        if (!str_uint32_hash_incr_by(vocab, token, count)) {
            log_error("Error adding phrases to vocabulary\n");
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }
    })

    kh_foreach(postal_code_counts, token, count, {
        if (!str_uint32_hash_incr_by(vocab, token, count)) {
            log_error("Error adding postal_codes to vocabulary\n");
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }
    })

    size_t hash_size;
    const char *context_token;
    bool sort_reverse = true;

    log_info("Creating phrase_types trie\n");

    sort_reverse = true;
    char **phrase_keys = str_uint32_hash_sort_keys_by_value(phrase_counts, sort_reverse);
    if (phrase_keys == NULL) {
        log_error("phrase_keys == NULL\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    hash_size = kh_size(phrase_counts);
    address_parser_types_array *phrase_types_array = address_parser_types_array_new_size(hash_size);

    for (size_t idx = 0; idx < hash_size; idx++) {
        char *phrase_key = phrase_keys[idx];
        khiter_t pk = kh_get(str_uint32, phrase_counts, phrase_key);
        if (pk == kh_end(phrase_counts)) {
            log_error("Key %zu did not exist in phrase_counts: %s\n", idx, phrase_key);
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }

        uint32_t phrase_count = kh_value(phrase_counts, pk);
        if (phrase_count < MIN_PHRASE_COUNT) {
            token = (char *)kh_key(phrase_counts, pk);
            kh_del(str_uint32, phrase_counts, pk);
            free((char *)token);
            continue;
        }

        k = kh_get(phrase_stats, phrase_stats, phrase_key);

        if (k == kh_end(phrase_stats)) {
            log_error("Key %zu did not exist in phrase_stats: %s\n", idx, phrase_key);
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }

        stats = kh_value(phrase_stats, k);

        place_class_counts = stats.class_counts;
        int32_t most_common = -1;
        uint32_t max_count = 0;
        uint32_t total = 0;
        for (uint32_t i = 0; i < NUM_ADDRESS_PARSER_BOUNDARY_TYPES; i++) {
            k = kh_get(int_uint32, place_class_counts, (khint_t)i);
            if (k != kh_end(place_class_counts)) {
                count = kh_value(place_class_counts, k);

                if (count > max_count) {
                    max_count = count;
                    most_common = i;
                }
                total += count;
            }
        }

        if (most_common > -1) {
            address_parser_types_t types;
            types.components = stats.components;
            types.most_common = (uint16_t)most_common;

            kh_value(phrase_counts, pk) = (uint32_t)phrase_types_array->n;
            address_parser_types_array_push(phrase_types_array, types);
        }
    }

    if (phrase_keys != NULL) {
        free(phrase_keys);
    }

    log_info("Creating phrases trie\n");

    parser->phrases = trie_new_from_hash(phrase_counts);
    if (parser->phrases == NULL) {
        log_error("Error converting phrase_counts to trie\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    if (phrase_types_array == NULL) {
        log_error("phrase_types_array is NULL\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    parser->phrase_types = phrase_types_array;

    char **postal_code_keys = str_uint32_hash_sort_keys_by_value(postal_code_counts, true);
    if (postal_code_keys == NULL) {
        log_error("postal_code_keys == NULL\n");
        free(phrase_keys);
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    log_info("Creating postal codes trie\n");

    hash_size = kh_size(postal_code_counts);
    for (size_t idx = 0; idx < hash_size; idx++) {
        char *phrase_key = postal_code_keys[idx];

        k = kh_get(str_uint32, postal_code_counts, phrase_key);
        if (k == kh_end(postal_code_counts)) {
            log_error("Key %zu did not exist in postal_code_counts: %s\n", idx, phrase_key);
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }
        uint32_t pc_count = kh_value(postal_code_counts, k);
        kh_value(postal_code_counts, k) = (uint32_t)idx;
    }

    if (postal_code_keys != NULL) {
        free(postal_code_keys);
    }

    parser->postal_codes = trie_new_from_hash(postal_code_counts);
    if (parser->postal_codes == NULL) {
        log_error("Error converting postal_code_counts to trie\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }

    log_info("Building postal code contexts\n");

    bool fixed_rows = false;
    graph_builder_t *postal_code_contexts_builder = graph_builder_new(GRAPH_BIPARTITE, fixed_rows);

    uint32_t postal_code_id;
    uint32_t context_phrase_id;

    khash_t(str_set) *context_phrases;

    kh_foreach(postal_code_admin_contexts, token, context_phrases, {
        if (!trie_get_data(parser->postal_codes, (char *)token, &postal_code_id)) {
            log_error("Key %s did not exist in parser->postal_codes\n", (char *)token);
            address_parser_destroy(parser);
            parser = NULL;
            goto exit_hashes_allocated;
        }
        kh_foreach_key(context_phrases, context_token, {
            if (!trie_get_data(parser->phrases, (char *)context_token, &context_phrase_id)) {
                log_error("Key %s did not exist in phrases trie\n", (char *)context_token);
                address_parser_destroy(parser);
                parser = NULL;
                goto exit_hashes_allocated;
            }

            graph_builder_add_edge(postal_code_contexts_builder, postal_code_id, context_phrase_id);
        })
    })

    bool sort_edges = true;
    bool remove_duplicates = true;
    graph_t *postal_code_contexts = graph_builder_finalize(postal_code_contexts_builder, sort_edges, remove_duplicates);

    // NOTE: don't destroy this during deallocation
    if (postal_code_contexts == NULL) {
        log_error("postal_code_contexts is NULL\n");
        address_parser_destroy(parser);
        parser = NULL;
        goto exit_hashes_allocated;
    }
    parser->postal_code_contexts = postal_code_contexts;

    log_info("Freeing memory from initialization\n");

exit_hashes_allocated:
    // Free memory for hashtables, etc.
    if (vocab_context != NULL) {
        char_array_destroy(vocab_context->token_builder);
        char_array_destroy(vocab_context->postal_code_token_builder);
        char_array_destroy(vocab_context->sub_token_builder);
        char_array_destroy(vocab_context->phrase_builder);
        phrase_array_destroy(vocab_context->dictionary_phrases);
        int64_array_destroy(vocab_context->phrase_memberships);
        phrase_array_destroy(vocab_context->postal_code_dictionary_phrases);
        token_array_destroy(vocab_context->sub_tokens);
        free(vocab_context);
    }

    cstring_array_destroy(phrases);
    cstring_array_destroy(phrase_labels);

    address_parser_data_set_destroy(data_set);

    if (phrase_counts_trie != NULL) {
        trie_destroy(phrase_counts_trie);
    }

    kh_foreach_key(vocab, token, {
        free((char *)token);
    })
    kh_destroy(str_uint32, vocab);

    kh_foreach_key(class_counts, token, {
        free((char *)token);
    })
    kh_destroy(str_uint32, class_counts);

    kh_foreach(phrase_stats, token, stats, {
        kh_destroy(int_uint32, stats.class_counts);
        free((char *)token);
    })

    kh_destroy(phrase_stats, phrase_stats);

    kh_foreach_key(phrase_counts, token, {
        free((char *)token);
    })

    kh_destroy(str_uint32, phrase_counts);

    kh_foreach_key(phrase_types, token, {
        free((char *)token);
    })
    kh_destroy(phrase_types, phrase_types);

    khash_t(str_set) *pc_set;

    kh_foreach(postal_code_admin_contexts, token, pc_set, {
        if (pc_set != NULL) {
            kh_foreach_key(pc_set, context_token, {
                free((char *)context_token);
            })
            kh_destroy(str_set, pc_set);
        }
        free((char *)token);
    })

    kh_destroy(postal_code_context_phrases, postal_code_admin_contexts);

    kh_foreach_key(postal_code_counts, token, {
        free((char *)token);
    })
    kh_destroy(str_uint32, postal_code_counts);

    return parser;
}

static inline bool address_parser_train_example(address_parser_t *self, void *trainer, address_parser_context_t *context, address_parser_data_set_t *data_set) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        return averaged_perceptron_trainer_train_example((averaged_perceptron_trainer_t *)trainer, self, context, context->features, context->prev_tag_features, context->prev2_tag_features, &address_parser_features, data_set->tokenized_str, data_set->labels);
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        return crf_averaged_perceptron_trainer_train_example((crf_averaged_perceptron_trainer_t *)trainer, self, context, context->features, context->prev_tag_features, &address_parser_features, data_set->tokenized_str, data_set->labels);
    } else {
        log_error("Parser model is of unknown type\n");
    }
    return false;
}

static inline void address_parser_trainer_destroy(address_parser_t *self, void *trainer) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_trainer_destroy((averaged_perceptron_trainer_t *)trainer);
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        crf_averaged_perceptron_trainer_destroy((crf_averaged_perceptron_trainer_t *)trainer);
    }
}

static inline bool address_parser_finalize_model(address_parser_t *self, void *trainer) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        self->model.ap = averaged_perceptron_trainer_finalize((averaged_perceptron_trainer_t *)trainer);
        return self->model.ap != NULL;
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        self->model.crf = crf_averaged_perceptron_trainer_finalize((crf_averaged_perceptron_trainer_t *)trainer);
        return self->model.crf != NULL;
    } else {
        log_error("Parser model is of unknown type\n");
    }
    return false;
}

static inline uint32_t address_parser_train_num_iterations(address_parser_t *self, void *trainer) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_trainer_t *ap_trainer = (averaged_perceptron_trainer_t *)trainer;
        return ap_trainer->iterations;
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        crf_averaged_perceptron_trainer_t *crf_trainer = (crf_averaged_perceptron_trainer_t *)trainer;
        return crf_trainer->iterations;
    } else {
        log_error("Parser model is of unknown type\n");
    }
    return 0;
}

static inline void address_parser_train_set_iterations(address_parser_t *self, void *trainer, uint32_t iterations) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_trainer_t *ap_trainer = (averaged_perceptron_trainer_t *)trainer;
        ap_trainer->iterations = iterations;
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        crf_averaged_perceptron_trainer_t *crf_trainer = (crf_averaged_perceptron_trainer_t *)trainer;
        crf_trainer->iterations = iterations;
    } else {
        log_error("Parser model is of unknown type\n");
    }
}

static inline uint64_t address_parser_train_num_errors(address_parser_t *self, void *trainer) {
    if (self->model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_trainer_t *ap_trainer = (averaged_perceptron_trainer_t *)trainer;
        return ap_trainer->num_updates;
    } else if (self->model_type == ADDRESS_PARSER_TYPE_CRF) {
        crf_averaged_perceptron_trainer_t *crf_trainer = (crf_averaged_perceptron_trainer_t *)trainer;
        return crf_trainer->num_updates;
    } else {
        log_error("Parser model is of unknown type\n");
    }
    return 0;
}

bool address_parser_train_epoch(address_parser_t *self, void *trainer, char *filename) {
    if (filename == NULL) {
        log_error("Filename was NULL\n");
        return false;
    }

    address_parser_data_set_t *data_set = address_parser_data_set_init(filename);
    if (data_set == NULL) {
        log_error("Error initializing data set\n");
        return false;
    }

    address_parser_context_t *context = self->context;

    size_t examples = 0;
    uint64_t errors = address_parser_train_num_errors(self, trainer);

    uint32_t iteration = address_parser_train_num_iterations(self, trainer);

    bool logged = false;

    while (address_parser_data_set_next(data_set)) {
        char *language = char_array_get_string(data_set->language);
        if (string_equals(language, UNKNOWN_LANGUAGE) || string_equals(language, AMBIGUOUS_LANGUAGE)) {
            language = NULL;
        }
        char *country = char_array_get_string(data_set->country);

        address_parser_context_fill(context, self, data_set->tokenized_str, language, country);

        bool example_success = address_parser_train_example(self, trainer, context, data_set);

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
            uint64_t prev_errors = errors;
            errors = address_parser_train_num_errors(self, trainer);

            log_info("Iter %d: Did %zu examples with %" PRIu64 " errors\n", iteration, examples, errors - prev_errors);
        }
    }

exit_epoch_training_started:
    address_parser_data_set_destroy(data_set);

    return true;
}


bool address_parser_train(address_parser_t *self, char *filename, address_parser_model_type_t model_type, uint32_t num_iterations, size_t min_updates) {
    self->model_type = model_type;
    void *trainer;
    if (model_type == ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON) {
        averaged_perceptron_trainer_t *ap_trainer = averaged_perceptron_trainer_new(min_updates);
        trainer = (void *)ap_trainer;
    } else if (model_type == ADDRESS_PARSER_TYPE_CRF) {
        crf_averaged_perceptron_trainer_t *crf_trainer = crf_averaged_perceptron_trainer_new(self->num_classes, min_updates);
        trainer = (void *)crf_trainer;
    }

    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        log_info("Doing epoch %d\n", iter);

        address_parser_train_set_iterations(self, trainer, iter);

        #if defined(HAVE_SHUF) || defined(HAVE_GSHUF)
        log_info("Shuffling\n");

        if (!shuffle_file_chunked_size(filename, DEFAULT_SHUFFLE_CHUNK_SIZE)) {
            log_error("Error in shuffle\n");
            address_parser_trainer_destroy(self, trainer);
            return false;
        }

        log_info("Shuffle complete\n");
        #endif
        
        if (!address_parser_train_epoch(self, trainer, filename)) {
            log_error("Error in epoch\n");
            address_parser_trainer_destroy(self, trainer);
            return false;
        }
    }

    log_debug("Done with training, averaging weights\n");

    if (!address_parser_finalize_model(self, trainer)) {
        log_error("model was NULL\n");
        return false;
    }

    return true;
}

typedef enum {
    ADDRESS_PARSER_TRAIN_POSITIONAL_ARG,
    ADDRESS_PARSER_TRAIN_ARG_ITERATIONS,
    ADDRESS_PARSER_TRAIN_ARG_MIN_UPDATES,
    ADDRESS_PARSER_TRAIN_ARG_MODEL_TYPE
} address_parser_train_keyword_arg_t;

#define USAGE "Usage: ./address_parser_train filename output_dir [--iterations number --min-updates number --model (crf|greedyap)]\n"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    #if !defined(HAVE_SHUF) && !defined(HAVE_GSHUF)
    log_warn("shuf must be installed to train address parser effectively. If this is a production machine, please install shuf. No shuffling will be performed.\n");
    #endif

    int pos_args = 1;
    
    address_parser_train_keyword_arg_t kwarg = ADDRESS_PARSER_TRAIN_POSITIONAL_ARG;

    size_t num_iterations = DEFAULT_ITERATIONS;
    uint64_t min_updates = DEFAULT_MIN_UPDATES;
    size_t position = 0;

    ssize_t arg_iterations;
    uint64_t arg_min_updates;

    char *filename = NULL;
    char *output_dir = NULL;

    address_parser_model_type_t model_type = DEFAULT_MODEL_TYPE;

    for (int i = pos_args; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--iterations")) {
            kwarg = ADDRESS_PARSER_TRAIN_ARG_ITERATIONS;
            continue;
        }

        if (string_equals(arg, "--min-updates")) {
            kwarg = ADDRESS_PARSER_TRAIN_ARG_MIN_UPDATES;
            continue;
        }

        if (string_equals(arg, "--model")) {
            kwarg = ADDRESS_PARSER_TRAIN_ARG_MODEL_TYPE;
            continue;
        }

        if (kwarg == ADDRESS_PARSER_TRAIN_ARG_ITERATIONS) {
            if (sscanf(arg, "%zd", &arg_iterations) != 1 || arg_iterations < 0) {
                log_error("Bad arg for --iterations: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            num_iterations = (size_t)arg_iterations;
        } else if (kwarg == ADDRESS_PARSER_TRAIN_ARG_MIN_UPDATES) {
            if (sscanf(arg, "%llu", &arg_min_updates) != 1) {
                log_error("Bad arg for --min-updates: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            min_updates = arg_min_updates;
            log_info("min_updates = %" PRIu64 "\n", min_updates);
        } else if (kwarg == ADDRESS_PARSER_TRAIN_ARG_MODEL_TYPE) {
            if (string_equals(arg, "crf")) {
                model_type = ADDRESS_PARSER_TYPE_CRF;
            } else if (string_equals(arg, "greedyap"))  {
                model_type = ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON;
            } else {
                log_error("Bad arg for --model, valid values are [crf, greedyap]\n");
                exit(EXIT_FAILURE);
            }
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

    address_parser_t *parser = address_parser_init(filename);

    if (parser == NULL) {
        log_error("Could not initialize parser\n");
        exit(EXIT_FAILURE);
    }

    log_info("Finished initialization\n");

    if (!address_parser_train(parser, filename, model_type, num_iterations, min_updates)) {
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
    log_debug("Done\n");
}
