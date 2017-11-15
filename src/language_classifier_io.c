#include "language_classifier_io.h"

#include "log/log.h"

#include "constants.h"
#include "collections.h"
#include "language_features.h"

language_classifier_data_set_t *language_classifier_data_set_init(char *filename) {
    language_classifier_data_set_t *data_set = malloc(sizeof(language_classifier_data_set_t));
    data_set->f = fopen(filename, "r");
    if (data_set->f == NULL) {
        free(data_set);
        return NULL;
    }

    data_set->tokens = token_array_new();
    data_set->feature_array = char_array_new();
    data_set->address = char_array_new();
    data_set->language = char_array_new_size(MAX_LANGUAGE_LEN);
    data_set->country = char_array_new_size(MAX_COUNTRY_CODE_LEN);

    return data_set;
}

bool language_classifier_data_set_next(language_classifier_data_set_t *self) {
    if (self == NULL) return false;

    char *line = file_getline(self->f);
    if (line == NULL) {
        return false;
    }

    size_t token_count;

    cstring_array *fields = cstring_array_split(line, TAB_SEPARATOR, TAB_SEPARATOR_LEN, &token_count);

    if (token_count != LANGUAGE_CLASSIFIER_FILE_NUM_TOKENS) {
        log_error("Token count did not match, expected %d, got %zu, line=%s\n", LANGUAGE_CLASSIFIER_FILE_NUM_TOKENS, token_count, line);
    }

    free(line);

    char *language = cstring_array_get_string(fields, LANGUAGE_CLASSIFIER_FIELD_LANGUAGE);
    char *country = cstring_array_get_string(fields, LANGUAGE_CLASSIFIER_FIELD_COUNTRY);
    char *address = cstring_array_get_string(fields, LANGUAGE_CLASSIFIER_FIELD_ADDRESS);

    log_debug("Doing: %s\n", address);

    char *normalized = language_classifier_normalize_string(address);
    bool is_normalized = normalized != NULL;
    if (!is_normalized) {
        log_debug("could not normalize\n");
        normalized = strdup(address);
    }

    char_array_clear(self->address);
    char_array_add(self->address, normalized);

    char_array_clear(self->country);
    char_array_add(self->country, country);

    char_array_clear(self->language);
    char_array_add(self->language, language);

    cstring_array_destroy(fields);
    bool ret = normalized != NULL;
    free(normalized);

    return ret;
}

void language_classifier_minibatch_destroy(language_classifier_minibatch_t *self) {
    if (self == NULL) return;

    size_t i;

    if (self->features != NULL) {
        for (i = 0; i < self->features->n; i++) {
            khash_t(str_double) *feature_counts = self->features->a[i];
            const char *feature;

            kh_foreach_key(feature_counts, feature, {
                free((char *)feature);
            })

            kh_destroy(str_double, feature_counts);
        }
        feature_count_array_destroy(self->features);

    }

    if (self->labels != NULL) {
        cstring_array_destroy(self->labels);
    }

    free(self);
}

language_classifier_minibatch_t *language_classifier_minibatch_new(void) {
    language_classifier_minibatch_t *minibatch = malloc(sizeof(language_classifier_minibatch_t));
    if (minibatch == NULL) return NULL;

    minibatch->features = feature_count_array_new();
    if (minibatch->features == NULL) {
        language_classifier_minibatch_destroy(minibatch);
        return NULL;
    }

    minibatch->labels = cstring_array_new();
    if (minibatch->labels == NULL) {
        language_classifier_minibatch_destroy(minibatch);
        return NULL;
    }

    return minibatch;
}

inline bool language_classifier_language_is_valid(char *language) {
    return !string_equals(language, AMBIGUOUS_LANGUAGE) && !string_equals(language, UNKNOWN_LANGUAGE);
}

language_classifier_minibatch_t *language_classifier_data_set_get_minibatch_with_size(language_classifier_data_set_t *self, khash_t(str_uint32) *labels, size_t batch_size) {
    size_t in_batch = 0;

    language_classifier_minibatch_t *minibatch = NULL;

    while (in_batch < batch_size && language_classifier_data_set_next(self)) {
        char *address = char_array_get_string(self->address);
        if (strlen(address) == 0) {
            continue;
        }

        char *country = NULL;
        //char *country = char_array_get_string(self->country);

        char *language = char_array_get_string(self->language);
        if (!language_classifier_language_is_valid(language)) {
            continue;
        }

        if (labels != NULL && kh_get(str_uint32, labels, language) == kh_end(labels)) {
            continue;
        }

        if (minibatch == NULL) {
            minibatch = language_classifier_minibatch_new();
            if (minibatch == NULL) {
                log_error("Error creating minibatch\n");
                return NULL;
            }
        }

        if (labels != NULL) {
            khash_t(str_double) *feature_counts = extract_language_features(address, country, self->tokens, self->feature_array);
            if (feature_counts == NULL) {
                log_error("Could not extract features for: %s\n", address);
                language_classifier_minibatch_destroy(minibatch);
                return NULL;
            }
            feature_count_array_push(minibatch->features, feature_counts);
        }
    
        cstring_array_add_string(minibatch->labels, language);
        in_batch++;
    }

    return minibatch;
}

inline language_classifier_minibatch_t *language_classifier_data_set_get_minibatch(language_classifier_data_set_t *self, khash_t(str_uint32) *labels) {
    return language_classifier_data_set_get_minibatch_with_size(self, labels, LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE);
}

void language_classifier_data_set_destroy(language_classifier_data_set_t *self) {
    if (self == NULL) return;

    if (self->f != NULL) {
        fclose(self->f);
    }

    if (self->tokens != NULL) {
        token_array_destroy(self->tokens);
    }

    if (self->feature_array != NULL) {
        char_array_destroy(self->feature_array);
    }

    if (self->address != NULL) {
        char_array_destroy(self->address);
    }

    if (self->language != NULL) {
        char_array_destroy(self->language);
    }

    if (self->country != NULL) {
        char_array_destroy(self->country);
    }

    free(self);
}