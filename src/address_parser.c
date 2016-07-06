#include "address_parser.h"
#include "address_dictionary.h"
#include "features.h"
#include "geodb.h"
#include "scanner.h"

#include "log/log.h"

#define ADDRESS_PARSER_MODEL_FILENAME "address_parser.dat"
#define ADDRESS_PARSER_VOCAB_FILENAME "address_parser_vocab.trie"
#define ADDRESS_PARSER_PHRASE_FILENAME "address_parser_phrases.trie"

#define UNKNOWN_WORD "UNKNOWN"
#define UNKNOWN_NUMERIC "UNKNOWN_NUMERIC"

static address_parser_t *parser = NULL;


address_parser_t *address_parser_new(void) {
    address_parser_t *parser = malloc(sizeof(address_parser_t));
    return parser;
}


address_parser_t *get_address_parser(void) {
    return parser;
}


bool address_parser_save(address_parser_t *self, char *output_dir) {
    if (self == NULL || output_dir == NULL) return false;

    char_array *path = char_array_new_size(strlen(output_dir));

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, output_dir, ADDRESS_PARSER_MODEL_FILENAME);
    char *model_path = char_array_get_string(path);

    if (!averaged_perceptron_save(self->model, model_path)) {
        char_array_destroy(path);
        return false;
    }

    char_array_clear(path);

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, output_dir, ADDRESS_PARSER_VOCAB_FILENAME);
    char *vocab_path = char_array_get_string(path);

    if (!trie_save(self->vocab, vocab_path)) {
        return false;
    }

    char_array_clear(path);

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, output_dir, ADDRESS_PARSER_PHRASE_FILENAME);
    char *phrases_path = char_array_get_string(path);

    if (!trie_save(self->phrase_types, phrases_path)) {
        return false;
    }

    char_array_destroy(path);

    return true;
}


bool address_parser_load(char *dir) {
    if (parser != NULL) return false;
    if (dir == NULL) {
        dir = LIBPOSTAL_ADDRESS_PARSER_DIR;
    }

    char_array *path = char_array_new_size(strlen(dir));

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, dir, ADDRESS_PARSER_MODEL_FILENAME);
    char *model_path = char_array_get_string(path);

    averaged_perceptron_t *model = averaged_perceptron_load(model_path);

    if (model == NULL) {
        char_array_destroy(path);
        return false;
    }

    parser = address_parser_new();
    parser->model = model;

    char_array_clear(path);

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, dir, ADDRESS_PARSER_VOCAB_FILENAME);

    char *vocab_path = char_array_get_string(path);

    trie_t *vocab = trie_load(vocab_path);

    if (vocab == NULL) {
        address_parser_destroy(parser);
        char_array_destroy(path);
        return false;
    }

    parser->vocab = vocab;

    char_array_clear(path);

    char_array_add_joined(path, PATH_SEPARATOR, true, 2, dir, ADDRESS_PARSER_PHRASE_FILENAME);

    char *phrases_path = char_array_get_string(path);

    trie_t *phrase_types = trie_load(phrases_path);

    if (phrase_types == NULL) {
        address_parser_destroy(parser);
        char_array_destroy(path);
        return false;
    }

    parser->phrase_types = phrase_types;

    char_array_destroy(path);
    return true;
}

void address_parser_destroy(address_parser_t *self) {
    if (self == NULL) return;

    if (self->model != NULL) {
        averaged_perceptron_destroy(self->model);
    }

    if (self->vocab != NULL) {
        trie_destroy(self->vocab);
    }

    if (self->phrase_types != NULL) {
        trie_destroy(self->phrase_types);
    }

    free(self);
}

static inline uint32_t word_vocab_frequency(address_parser_t *parser, char *word) {   
    uint32_t count = 0;
    bool has_key = trie_get_data(parser->vocab, word, &count);
    return count;
}

inline void address_parser_normalize_token(cstring_array *array, char *str, token_t token) {
    normalize_token(array, str, token, ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS);
}

inline char *address_parser_normalize_string(char *str) {
    return normalize_string_latin(str, strlen(str), ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS);
}


void address_parser_context_destroy(address_parser_context_t *self) {
    if (self == NULL) return;

    if (self->phrase != NULL) {
        char_array_destroy(self->phrase);
    }

    if (self->component_phrase != NULL) {
        char_array_destroy(self->component_phrase);
    }

    if (self->geodb_phrase != NULL) {
        char_array_destroy(self->geodb_phrase);
    }

    if (self->separators != NULL) {
        uint32_array_destroy(self->separators);
    }

    if (self->normalized != NULL) {
        cstring_array_destroy(self->normalized);
    }

    if (self->features != NULL) {
        cstring_array_destroy(self->features);
    }

    if (self->tokenized_str != NULL) {
        tokenized_string_destroy(self->tokenized_str);
    }

    if (self->address_dictionary_phrases != NULL) {
        phrase_array_destroy(self->address_dictionary_phrases);
    }

    if (self->address_phrase_memberships != NULL) {
        int64_array_destroy(self->address_phrase_memberships);
    }

    if (self->geodb_phrases != NULL) {
        phrase_array_destroy(self->geodb_phrases);
    }

    if (self->geodb_phrase_memberships != NULL) {
        int64_array_destroy(self->geodb_phrase_memberships);
    }

    if (self->component_phrases != NULL) {
        phrase_array_destroy(self->component_phrases);
    }

    if (self->component_phrase_memberships != NULL) {
        int64_array_destroy(self->component_phrase_memberships);
    }

    free(self);
}

address_parser_context_t *address_parser_context_new(void) {
    address_parser_context_t *context = malloc(sizeof(address_parser_context_t));

    if (context == NULL) return NULL;

    context->language = NULL;
    context->country = NULL;

    context->phrase = char_array_new();
    if (context->phrase == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->component_phrase = char_array_new();
    if (context->component_phrase == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->geodb_phrase = char_array_new();
    if (context->geodb_phrase == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->separators = uint32_array_new();
    if (context->separators == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->normalized = cstring_array_new();
    if (context->normalized == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->features = cstring_array_new();
    if (context->features == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->tokenized_str = tokenized_string_new();
    if (context->tokenized_str == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->address_dictionary_phrases = phrase_array_new();
    if (context->address_dictionary_phrases == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->address_phrase_memberships = int64_array_new();
    if (context->address_phrase_memberships == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->geodb_phrases = phrase_array_new();
    if (context->geodb_phrases == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->geodb_phrase_memberships = int64_array_new();
    if (context->geodb_phrase_memberships == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->component_phrases = phrase_array_new();
    if (context->component_phrases == NULL) {
        goto exit_address_parser_context_allocated;
    }

    context->component_phrase_memberships = int64_array_new();
    if (context->component_phrase_memberships == NULL) {
        goto exit_address_parser_context_allocated;
    }

    return context;

exit_address_parser_context_allocated:
    address_parser_context_destroy(context);
    return NULL;
}

void address_parser_context_fill(address_parser_context_t *context, address_parser_t *parser, tokenized_string_t *tokenized_str, char *language, char *country) {
    int64_t i, j;

    uint32_t token_index;
    char *word;
    phrase_t phrase;

    context->language = language;
    context->country = country;

    cstring_array *normalized = context->normalized;
    cstring_array_clear(normalized);

    char *str = tokenized_str->str;
    token_array *tokens = tokenized_str->tokens;

    cstring_array_foreach(tokenized_str->strings, token_index, word, {
        token_t token = tokens->a[token_index];
        address_parser_normalize_token(normalized, str, token);
    })

    phrase_array_clear(context->address_dictionary_phrases);
    int64_array_clear(context->address_phrase_memberships);
    
    i = 0;
    phrase_array *address_dictionary_phrases = context->address_dictionary_phrases;
    int64_array *address_phrase_memberships = context->address_phrase_memberships;

    if (search_address_dictionaries_tokens_with_phrases(str, tokens, context->language, &context->address_dictionary_phrases)) {
        for (j = 0; j < address_dictionary_phrases->n; j++) {
            phrase = address_dictionary_phrases->a[j];

            for (; i < phrase.start; i++) {
                int64_array_push(address_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
                log_debug("token i=%lld, null phrase membership\n", i);
            }

            for (i = phrase.start; i < phrase.start + phrase.len; i++) {
                log_debug("token i=%lld, phrase membership=%lld\n", i, j);
                int64_array_push(address_phrase_memberships, j);
            }
        }
    }

    for (; i < tokens->n; i++) {
        log_debug("token i=%lld, null phrase membership\n", i);
        int64_array_push(address_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
    }

    phrase_array_clear(context->geodb_phrases);
    int64_array_clear(context->geodb_phrase_memberships);

    phrase_array *geodb_phrases = context->geodb_phrases;
    int64_array *geodb_phrase_memberships = context->geodb_phrase_memberships;
    i = 0;

    if (search_geodb_tokens_with_phrases(str, tokens, &context->geodb_phrases)) {
        for (j = 0; j < geodb_phrases->n; j++) {
            phrase = geodb_phrases->a[j];

            for (; i < phrase.start; i++) {
                log_debug("token i=%lld, null geo phrase membership\n", i);
                int64_array_push(geodb_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
            }

            for (i = phrase.start; i < phrase.start + phrase.len; i++) {
                log_debug("token i=%lld, geo phrase membership=%lld\n", i, j);
                int64_array_push(geodb_phrase_memberships, j);
            }
        }
    }
    for (; i < tokens->n; i++) {
        log_debug("token i=%lld, null geo phrase membership\n", i);
        int64_array_push(geodb_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
    }

    phrase_array_clear(context->component_phrases);
    int64_array_clear(context->component_phrase_memberships);
    i = 0;

    phrase_array *component_phrases = context->component_phrases;
    int64_array *component_phrase_memberships = context->component_phrase_memberships;

    if (trie_search_tokens_with_phrases(parser->phrase_types, str, tokens, &component_phrases)) {
        for (j = 0; j < component_phrases->n; j++) {
            phrase = component_phrases->a[j];

            for (; i < phrase.start; i++) {
                log_debug("token i=%lld, null component phrase membership\n", i);
                int64_array_push(component_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
            }

            for (i = phrase.start; i < phrase.start + phrase.len; i++) {
                log_debug("token i=%lld, component phrase membership=%lld\n", i, j);
                int64_array_push(component_phrase_memberships, j);
            }
        }
    }
    for (; i < tokens->n; i++) {
        log_debug("token i=%lld, null component phrase membership\n", i);
        int64_array_push(component_phrase_memberships, NULL_PHRASE_MEMBERSHIP);
    }



}


static inline char *get_phrase_string(tokenized_string_t *str, char_array *phrase_tokens, phrase_t phrase) {
    size_t phrase_len = 0;
    char_array_clear(phrase_tokens);

    size_t phrase_end = phrase.start + phrase.len;

    for (int k = phrase.start; k < phrase_end; k++) {
        char *w = tokenized_string_get_token(str, k);
        char_array_append(phrase_tokens, w);
        if (k < phrase_end - 1) {
            char_array_append(phrase_tokens, " ");
        }
    }
    char_array_terminate(phrase_tokens);

    return char_array_get_string(phrase_tokens);
}


/*

typedef struct adjacent_phrase {
    phrase_t phrase;
    uint32_t num_separators;
} adjacent_phrase_t;

#define NULL_ADJACENT_PHRASE (adjacent_phrase_t){NULL_PHRASE, 0};

static inline adjacent_phrase_t get_adjacent_phrase(int64_array *phrase_memberships, phrase_array *phrases, uint32_array *separator_positions, uint32_t i, int32_t direction) {
    uint32_t *separators = separator_positions->a;
    int64_t *memberships = phrase_memberships->a;

    uint32_t num_strings = (uint32_t)phrase_memberships->n;

    adjacent_phrase_t adjacent = NULL_ADJACENT_PHRASE;

    if (direction == -1) { 
        for (uint32_t idx = i; idx >= 0; idx--) {
            uint32_t separator = separators[idx];
            if (separator > ADDRESS_SEPARATOR_NONE) {
                adjacent.num_separators++;
            }

            int64_t membership = memberships[ids];
            if (membership != NULL_PHRASE_MEMBERSHIP) {
                adjacent.phrase = phrases->a[membership];
                break;
            }

        }
    } else if (direction == 1) {
        for (uint32_t idx = i; idx < num_strings; idx++) {
            uint32_t separator = separators[idx];
            if (separator > ADDRESS_SEPARATOR_NONE) {
                adjacent.num_separators++;
            }

            int64_t membership = memberships[ids];
            if (membership != NULL_PHRASE_MEMBERSHIP) {
                adjacent.phrase = phrases->a[membership];
                break;
            }
        }
    }

    return adjacent;
}
*/

static inline void add_phrase_features(cstring_array *features, uint32_t phrase_types, uint32_t component, char *phrase_type, char *phrase_string, char *prev2, char *prev) {
    if (phrase_types == component) {
        log_debug("phrase=%s, phrase_types=%d\n", phrase_string, phrase_types);
        feature_array_add(features, 2, "unambiguous phrase type", phrase_type);
        feature_array_add(features, 3, "unambiguous phrase type+phrase", phrase_type, phrase_string);
    } else if (phrase_types & component) {
        feature_array_add(features, 3, "phrase type+phrase", phrase_type, phrase_string);
    }
}

/*
address_parser_features
-----------------------

This is a feature function similar to those found in MEMM and CRF models.

Follows the signature of an ap_feature_function so it can be called
as a function pointer by the averaged perceptron model.

Parameters:

address_parser_t *self: a pointer to the address_parser struct, which contains
word frequencies and perhaps other useful corpus-wide statistics.

address_parser_context_t *context: The context struct containing:
- phrase dictionary memberships for all the tokens
- country (if knkown)
- language (if known)
- features array

tokenized_string_t *tokenized: the sequence of tokens for parsing
uint32_t i: the current token index
char *prev: the predicted tag at index i - 1
char *prev2: the predicted tag at index i - 2

*/

bool address_parser_features(void *self, void *ctx, tokenized_string_t *tokenized, uint32_t i, char *prev, char *prev2) {
    if (self == NULL || ctx == NULL) return false;

    address_parser_t *parser = (address_parser_t *)self;
    address_parser_context_t *context = (address_parser_context_t *)ctx;

    cstring_array *features = context->features;
    char *language = context->language;
    char *country = context->country;

    phrase_array *address_dictionary_phrases = context->address_dictionary_phrases;
    int64_array *address_phrase_memberships = context->address_phrase_memberships;
    phrase_array *geodb_phrases = context->geodb_phrases;
    int64_array *geodb_phrase_memberships = context->geodb_phrase_memberships;
    phrase_array *component_phrases = context->component_phrases;
    int64_array *component_phrase_memberships = context->component_phrase_memberships;
    cstring_array *normalized = context->normalized;

    uint32_array *separators = context->separators;

    cstring_array_clear(features);

    token_t token = tokenized->tokens->a[i];

    ssize_t last_index = (ssize_t)i - 1;
    ssize_t next_index = (ssize_t)i + 1;

    char *word = cstring_array_get_string(normalized, i);
    if (word == NULL) {
        log_error("got NULL word at %d\n", i);
        return false;
    }

    size_t word_len = strlen(word);

    log_debug("word=%s\n", word);

    expansion_value_t expansion;

    phrase_t phrase = NULL_PHRASE;

    char *phrase_string = NULL;
    char *geo_phrase_string = NULL;
    char *component_phrase_string = NULL;

    int64_t address_phrase_index = address_phrase_memberships->a[i];

    char_array *phrase_tokens = context->phrase;
    char_array *component_phrase_tokens = context->component_phrase;
    char_array *geodb_phrase_tokens = context->geodb_phrase;

    bool add_word_feature = true;

    // Address dictionary phrases
    if (address_phrase_index != NULL_PHRASE_MEMBERSHIP) {
        phrase = address_dictionary_phrases->a[address_phrase_index];
        log_debug("phrase\n");

        last_index = (ssize_t)phrase.start - 1;
        next_index = (ssize_t)phrase.start + phrase.len;

        expansion.value = phrase.data;
        uint32_t address_phrase_types = expansion.components;

        log_debug("expansion=%d\n", expansion.value);

        if (address_phrase_types & (ADDRESS_STREET | ADDRESS_NAME)) {
            phrase_string = get_phrase_string(tokenized, phrase_tokens, phrase);

            add_word_feature = false;
            log_debug("phrase_string=%s\n", phrase_string);

            add_phrase_features(features, address_phrase_types, ADDRESS_STREET, "street", phrase_string, prev2, prev);
            add_phrase_features(features, address_phrase_types, ADDRESS_NAME, "name", phrase_string, prev2, prev);
        }
    }

    // Prefixes like hinter, etc.
    phrase_t prefix_phrase = search_address_dictionaries_prefix(word, token.len, language);
    if (prefix_phrase.len > 0) {
        expansion.value = prefix_phrase.data;
        // Don't include elisions like l', d', etc. which are in the ADDRESS_ANY category
        if (expansion.components ^ ADDRESS_ANY) {
            char_array_clear(phrase_tokens);
            char_array_add_len(phrase_tokens, word, prefix_phrase.len);
            char *prefix = char_array_get_string(phrase_tokens);
            log_debug("got prefix: %s\n", prefix);
            feature_array_add(features, 2, "prefix", prefix);
        }
    }

    // Suffixes like straße, etc.
    phrase_t suffix_phrase = search_address_dictionaries_suffix(word, token.len, language);
    if (suffix_phrase.len > 0) {
        expansion.value = suffix_phrase.data;
        if (expansion.components & ADDRESS_STREET) {
            char_array_clear(phrase_tokens);
            char_array_add_len(phrase_tokens, word + (token.len - suffix_phrase.len), suffix_phrase.len);
            char *suffix = char_array_get_string(phrase_tokens);
            log_debug("got suffix: %s\n", suffix);
            feature_array_add(features, 2, "suffix", suffix);
        }
    }

    int64_t component_phrase_index = component_phrase_memberships->a[i];
    phrase = NULL_PHRASE;

    address_parser_types_t types;

    // Component phrases
    if (component_phrase_index != NULL_PHRASE_MEMBERSHIP) {
        phrase = component_phrases->a[component_phrase_index];

        component_phrase_string = get_phrase_string(tokenized, component_phrase_tokens, phrase);
        
        types.value = phrase.data;
        uint32_t component_phrase_types = types.components;
        uint32_t most_common = types.most_common;

        if (last_index >= (ssize_t)phrase.start - 1 || next_index <= (ssize_t)phrase.start + phrase.len - 1) {
            last_index = (ssize_t)phrase.start - 1;
            next_index = (ssize_t)phrase.start + phrase.len;

        }

        if (component_phrase_string != NULL && component_phrase_types ^ ADDRESS_COMPONENT_POSTAL_CODE) {
            feature_array_add(features, 2, "phrase", component_phrase_string);
            add_word_feature = false;
        }

        if (component_phrase_types > 0) {
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_SUBURB, "suburb", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_CITY, "city", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_CITY_DISTRICT, "city_district", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_STATE_DISTRICT, "state_district", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_STATE, "state", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_POSTAL_CODE, "postal_code", component_phrase_string, prev2, prev);
            add_phrase_features(features, component_phrase_types, ADDRESS_COMPONENT_COUNTRY, "country", component_phrase_string, prev2, prev);
        }

        if (most_common == ADDRESS_PARSER_CITY) {
            feature_array_add(features, 2, "commonly city", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_STATE) {
            feature_array_add(features, 2, "commonly state", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_COUNTRY) {
            feature_array_add(features, 2, "commonly country", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_STATE_DISTRICT) {
            feature_array_add(features, 2, "commonly state_district", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_SUBURB) {
            feature_array_add(features, 2, "commonly suburb", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_CITY_DISTRICT) {
            feature_array_add(features, 2, "commonly city_district", component_phrase_string);
        } else if (most_common == ADDRESS_PARSER_POSTAL_CODE) {
            feature_array_add(features, 2, "commonly postal_code", component_phrase_string);
        }

    }

    int64_t geodb_phrase_index = geodb_phrase_memberships->a[i];

    phrase = NULL_PHRASE;
    geodb_value_t geo;

    // GeoDB phrases
    if (component_phrase_index == NULL_PHRASE_MEMBERSHIP && geodb_phrase_index != NULL_PHRASE_MEMBERSHIP) {
        phrase = geodb_phrases->a[geodb_phrase_index];

        geo_phrase_string = get_phrase_string(tokenized, geodb_phrase_tokens, phrase);
        geo.value = phrase.data;
        uint32_t geodb_phrase_types = geo.components;

        if (last_index >= (ssize_t)phrase.start - 1 || next_index <= (ssize_t)phrase.start + phrase.len) {
            last_index = (ssize_t)phrase.start - 1;
            next_index = (ssize_t)phrase.start + phrase.len;
        }

        if (geo_phrase_string != NULL && geodb_phrase_types ^ ADDRESS_POSTAL_CODE) {
            feature_array_add(features, 2, "phrase", geo_phrase_string);
            add_word_feature = false;
        }

        if (geodb_phrase_types ^ ADDRESS_ANY) {
            add_phrase_features(features, geodb_phrase_types, ADDRESS_LOCALITY, "gn city", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_ADMIN1, "gn admin1", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_ADMIN2, "gn admin2", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_ADMIN3, "gn admin3", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_ADMIN4, "gn admin4", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_ADMIN_OTHER, "gn admin other", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_NEIGHBORHOOD, "gn neighborhood", geo_phrase_string, prev2, prev);

            add_phrase_features(features, geodb_phrase_types, ADDRESS_COUNTRY, "gn country", geo_phrase_string, prev2, prev);
            add_phrase_features(features, geodb_phrase_types, ADDRESS_POSTAL_CODE, "gn postal code", geo_phrase_string, prev2, prev);

        }

    }

    uint32_t word_freq = word_vocab_frequency(parser, word);

    if (add_word_feature) {
        // Bias unit, acts as an intercept
        feature_array_add(features, 1, "bias");

        if (word_freq > 0) {
            // The individual word
            feature_array_add(features, 2, "word", word);
        } else {
            log_debug("word not in vocab: %s\n", word);
            word = (token.type != NUMERIC && token.type != IDEOGRAPHIC_NUMBER) ? UNKNOWN_WORD : UNKNOWN_NUMERIC;
        }
    } else if (component_phrase_string != NULL) {
        word = component_phrase_string;
    } else if (geo_phrase_string != NULL) {
        word = geo_phrase_string;
    } else if (phrase_string != NULL) {
        word = phrase_string;
    }

    if (prev != NULL && last_index == i - 1) {
        // Previous tag and current word
        feature_array_add(features, 3, "i-1 tag+word", prev, word);
        feature_array_add(features, 2, "i-1 tag", prev);

        if (prev2 != NULL) {
            // Previous two tags and current word
            feature_array_add(features, 4, "i-2 tag+i-1 tag+word", prev2, prev, word);
            feature_array_add(features, 3, "i-2 tag+i-1 tag", prev2, prev);
        }
    }

    if (last_index >= 0) {
        char *prev_word = cstring_array_get_string(normalized, last_index);

        uint32_t prev_word_freq = word_vocab_frequency(parser, prev_word);
        if (prev_word_freq == 0) {
            token_t prev_token = tokenized->tokens->a[last_index];
            prev_word = (prev_token.type != NUMERIC && prev_token.type != IDEOGRAPHIC_NUMBER) ? UNKNOWN_WORD : UNKNOWN_NUMERIC;
        }

        // Previous word
        feature_array_add(features, 2, "i-1 word", prev_word);

        if (last_index == i - 1) {
            feature_array_add(features, 3, "i-1 tag+i-1 word", prev, prev_word);
        }

        // Previous word and current word
        feature_array_add(features, 3, "i-1 word+word", prev_word, word);
    }

    size_t num_tokens = tokenized->tokens->n;

    if (next_index < num_tokens) {
        char *next_word = cstring_array_get_string(normalized, next_index);

        uint32_t next_word_freq = word_vocab_frequency(parser, next_word);
        if (next_word_freq == 0) {
            token_t next_token = tokenized->tokens->a[next_index];
            next_word = (next_token.type != NUMERIC && next_token.type != IDEOGRAPHIC_NUMBER) ? UNKNOWN_WORD : UNKNOWN_NUMERIC;
        }

        // Next word e.g. if the current word is unknown and the next word is "street"
        feature_array_add(features, 2, "i+1 word", next_word);

        // Current word and next word
        feature_array_add(features, 3, "word+i+1 word", word, next_word);
    }

    #ifndef PRINT_FEATURES
    if (0) {
    #endif

    uint32_t idx;
    char *feature;

    printf("{");
    cstring_array_foreach(features, idx, feature, {
        printf("  %s, ", feature);
    })
    printf("}\n");

    #ifndef PRINT_FEATURES
    }
    #endif

    return true;

}

address_parser_response_t *address_parser_response_new(void) {
    address_parser_response_t *response = malloc(sizeof(address_parser_response_t));
    return response;
}

address_parser_response_t *address_parser_parse(char *address, char *language, char *country, address_parser_context_t *context) {
    if (address == NULL || context == NULL) return NULL;

    address_parser_t *parser = get_address_parser();
    if (parser == NULL) {
        log_error("parser is not setup, call libpostal_setup_address_parser()\n");
        return NULL;
    }

    char *normalized = address_parser_normalize_string(address);
    bool is_normalized = normalized != NULL;
    if (!is_normalized) {
        normalized = address;
    }

    averaged_perceptron_t *model = parser->model;

    token_array *tokens = tokenize(normalized);

    tokenized_string_t *tokenized_str = tokenized_string_new_from_str_size(normalized, strlen(normalized), tokens->n);

    for (int i = 0; i < tokens->n; i++) {
        token_t token = tokens->a[i];
        if (ADDRESS_PARSER_IS_SEPARATOR(token.type)) {
            uint32_array_push(context->separators, ADDRESS_SEPARATOR_FIELD_INTERNAL);
            continue;
        } else if (ADDRESS_PARSER_IS_IGNORABLE(token.type)) {
            continue;
        }

        tokenized_string_add_token(tokenized_str, (const char *)normalized, token.len, token.type, token.offset);
        uint32_array_push(context->separators, ADDRESS_SEPARATOR_NONE);
    }

    // This parser was trained without knowing language/country.
    // If at some point we build country-specific/language-specific
    // parsers, these parameters could be used to select a model.
    // The language parameter does technically control which dictionaries
    // are searched at the street level. It's possible with e.g. a phrase
    // like "de", which can be either the German country code or a stopword
    // in Spanish, that even in the case where it's being used as a country code,
    // it's possible that both the street-level and admin-level phrase features
    // may be working together as a kind of intercept. Depriving the model
    // of the street-level phrase features by passing in a known language
    // may change the decision threshold so explicitly ignore these
    // options until there's a use for them (country-specific or language-specific
    // parser models).

    language = NULL;
    country = NULL;
    address_parser_context_fill(context, parser, tokenized_str, language, country);

    address_parser_response_t *response = NULL;

    // If the whole input string is a single known phrase at the SUBURB level or higher, bypass sequence prediction altogether

    if (context->component_phrases->n == 1) {
        phrase_t only_phrase = context->component_phrases->a[0];
        if (only_phrase.start == 0 && only_phrase.len == tokenized_str->tokens->n) {
            address_parser_types_t types;

            types.value = only_phrase.data;
            uint32_t most_common = types.most_common;

            char *label;

            response = address_parser_response_new();

            if (most_common == ADDRESS_PARSER_CITY) {
                label = strdup(ADDRESS_PARSER_LABEL_CITY);
            } else if (most_common == ADDRESS_PARSER_STATE) {
                label = strdup(ADDRESS_PARSER_LABEL_STATE);
            } else if (most_common == ADDRESS_PARSER_COUNTRY) {
                label = strdup(ADDRESS_PARSER_LABEL_COUNTRY);
            } else if (most_common == ADDRESS_PARSER_STATE_DISTRICT) {
                label = strdup(ADDRESS_PARSER_LABEL_STATE_DISTRICT);
            } else if (most_common == ADDRESS_PARSER_SUBURB) {
                label = strdup(ADDRESS_PARSER_LABEL_SUBURB);
            } else if (most_common == ADDRESS_PARSER_CITY_DISTRICT) {
                label = strdup(ADDRESS_PARSER_LABEL_CITY_DISTRICT);
            } else if (most_common == ADDRESS_PARSER_POSTAL_CODE) {
                label = strdup(ADDRESS_PARSER_LABEL_POSTAL_CODE);
            }

            char **single_label = malloc(sizeof(char *));
            single_label[0] = label;
            char **single_component = malloc(sizeof(char *));
            single_component[0] = strdup(normalized);

            response->num_components = 1;
            response->labels = single_label;
            response->components = single_component;

            token_array_destroy(tokens);
            tokenized_string_destroy(tokenized_str);
            return response;
        }
    }

    cstring_array *token_labels = cstring_array_new_size(tokens->n);

    char *prev_label = NULL;

    if (averaged_perceptron_tagger_predict(model, parser, context, context->features, token_labels, &address_parser_features, tokenized_str)) {
        response = address_parser_response_new();

        size_t num_strings = cstring_array_num_strings(tokenized_str->strings);

        cstring_array *labels = cstring_array_new_size(num_strings);
        cstring_array *components = cstring_array_new_size(strlen(address) + num_strings);


        for (int i = 0; i < num_strings; i++) {
            char *str = tokenized_string_get_token(tokenized_str, i);
            char *label = cstring_array_get_string(token_labels, i);

            if (prev_label == NULL || strcmp(label, prev_label) != 0) {
                cstring_array_add_string(labels, label);
                cstring_array_start_token(components);

            }

            if (prev_label != NULL && strcmp(label, prev_label) == 0) {
                cstring_array_cat_string(components, " ");
                cstring_array_cat_string(components, str);
            } else {
                cstring_array_append_string(components, str);
                cstring_array_terminate(components);
            }

            prev_label = label;
        }
        response->num_components = cstring_array_num_strings(components);
        response->components = cstring_array_to_strings(components);
        response->labels = cstring_array_to_strings(labels);

    }

    token_array_destroy(tokens);
    tokenized_string_destroy(tokenized_str);
    cstring_array_destroy(token_labels);

    return response;
}



bool address_parser_module_setup(char *dir) {
    if (parser == NULL) {
        return address_parser_load(dir);
    }
    return true;
}

void address_parser_module_teardown(void) {
    if (parser != NULL) {
        address_parser_destroy(parser);
    }
    parser = NULL;
}
