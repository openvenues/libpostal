#include "language_classifier.h"

#include <float.h>

#include "language_features.h"
#include "minibatch.h"
#include "normalize.h"
#include "token_types.h"
#include "unicode_scripts.h"

#define LANGUAGE_CLASSIFIER_SIGNATURE 0xCCCCCCCC

#define LANGUAGE_CLASSIFIER_SETUP_ERROR "language_classifier not loaded, run libpostal_setup_language_classifier()\n"

#define MIN_PROB (0.05 - DBL_EPSILON)

static language_classifier_t *language_classifier = NULL;

void language_classifier_destroy(language_classifier_t *self) {
    if (self == NULL) return;

    if (self->features != NULL) {
        trie_destroy(self->features);
    }

    if (self->labels != NULL) {
        cstring_array_destroy(self->labels);
    }

    if (self->weights != NULL) {
        matrix_destroy(self->weights);
    }

    free(self);
}

language_classifier_t *language_classifier_new(void) {
    language_classifier_t *language_classifier = malloc(sizeof(language_classifier_t));
    return language_classifier;
}

language_classifier_t *get_language_classifier(void) {
    return language_classifier;
}

void language_classifier_response_destroy(language_classifier_response_t *self) {
    if (self == NULL) return;
    if (self->languages != NULL) {
        free(self->languages);
    }

    if (self->probs) {
        free(self->probs);
    }

    free(self);
}

language_classifier_response_t *classify_languages(char *address) {
    language_classifier_t *classifier = get_language_classifier();
    
    if (classifier == NULL) {
        log_error(LANGUAGE_CLASSIFIER_SETUP_ERROR);
        return NULL;
    }

    char *normalized = language_classifier_normalize_string(address);

    token_array *tokens = token_array_new();
    char_array *feature_array = char_array_new();

    khash_t(str_double) *feature_counts = extract_language_features(normalized, NULL, tokens, feature_array);
    if (feature_counts == NULL || kh_size(feature_counts) == 0) {
        token_array_destroy(tokens);
        char_array_destroy(feature_array);
        if (feature_counts != NULL) {
            kh_destroy(str_double, feature_counts);
        }
        free(normalized);
        return NULL;
    }

    sparse_matrix_t *x = feature_vector(classifier->features, feature_counts);

    size_t n = classifier->num_labels;
    matrix_t *p_y = matrix_new_zeros(1, n);

    language_classifier_response_t *response = NULL;
    if (logistic_regression_model_expectation(classifier->weights, x, p_y)) {
        double *predictions = matrix_get_row(p_y, 0);
        size_t *indices = double_array_argsort(predictions, n);
        size_t num_languages = 0;
        size_t i;
        double prob;

        double min_prob = 1.0 / n;
        if (min_prob < MIN_PROB) min_prob = MIN_PROB;

        for (i = 0; i < n; i++) {
            size_t idx = indices[n - i - 1];
            prob = predictions[idx];

            if (i == 0 || prob > min_prob) {
                num_languages++;
            } else {
                break;
            }
        }
        char **languages = malloc(sizeof(char *) * num_languages);
        double *probs = malloc(sizeof(double) * num_languages);

        for (i = 0; i < num_languages; i++) {
            size_t idx = indices[n - i - 1];
            char *lang = cstring_array_get_string(classifier->labels, (uint32_t)idx);
            prob = predictions[idx];
            languages[i] = lang;
            probs[i] = prob;
        }

        free(indices);

        response = malloc(sizeof(language_classifier_response_t));
        response->num_languages = num_languages;
        response->languages = languages;
        response->probs = probs;
    }

    sparse_matrix_destroy(x);
    matrix_destroy(p_y);
    token_array_destroy(tokens);
    char_array_destroy(feature_array);
    const char *key;
    kh_foreach_key(feature_counts, key, {
        free((char *)key);
    })
    kh_destroy(str_double, feature_counts);
    free(normalized);
    return response;

}

language_classifier_t *language_classifier_read(FILE *f) {
    if (f == NULL) return NULL;
    long save_pos = ftell(f);

    uint32_t signature;

    if (!file_read_uint32(f, &signature) || signature != LANGUAGE_CLASSIFIER_SIGNATURE) {
        goto exit_file_read;
    }

    language_classifier_t *classifier = language_classifier_new();
    if (classifier == NULL) {
        goto exit_file_read;
    }

    trie_t *features = trie_read(f);
    if (features == NULL) {
        goto exit_classifier_created;
    }
    classifier->features = features;
    uint64_t num_features;
    if (!file_read_uint64(f, &num_features)) {
        goto exit_classifier_created;
    }
    classifier->num_features = (size_t)num_features;

    uint64_t labels_str_len;

    if (!file_read_uint64(f, &labels_str_len)) {
        goto exit_classifier_created;
    }

    char_array *array = char_array_new_size(labels_str_len);

    if (array == NULL) {
        goto exit_classifier_created;
    }

    if (!file_read_chars(f, array->a, labels_str_len)) {
        char_array_destroy(array);
        goto exit_classifier_created;
    }

    array->n = labels_str_len;

    classifier->labels = cstring_array_from_char_array(array);
    if (classifier->labels == NULL) {
        goto exit_classifier_created;
    }
    classifier->num_labels = cstring_array_num_strings(classifier->labels);

    matrix_t *weights = matrix_read(f);

    if (weights == NULL) {
        goto exit_classifier_created;
    }

    classifier->weights = weights;

    return classifier;

exit_classifier_created:
    language_classifier_destroy(classifier);
exit_file_read:
    fseek(f, save_pos, SEEK_SET);
    return NULL;
}


language_classifier_t *language_classifier_load(char *path) {
    FILE *f;

    f = fopen(path, "rb");
    if (!f) return NULL;

    language_classifier_t *classifier = language_classifier_read(f);

    fclose(f);
    return classifier;
}

bool language_classifier_write(language_classifier_t *self, FILE *f) {
    if (f == NULL || self == NULL) return false;

    if (!file_write_uint32(f, LANGUAGE_CLASSIFIER_SIGNATURE) ||
        !trie_write(self->features, f) ||
        !file_write_uint64(f, self->num_features) ||
        !file_write_uint64(f, self->labels->str->n) ||
        !file_write_chars(f, (const char *)self->labels->str->a, self->labels->str->n) ||
        !matrix_write(self->weights, f)) {
        return false;
    }

    return true;
}

bool language_classifier_save(language_classifier_t *self, char *path) {
    if (self == NULL || path == NULL) return false;

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    bool result = language_classifier_write(self, f);
    fclose(f);

    return result;
}

// Module setup/teardown

bool language_classifier_module_setup(char *dir) {
    if (language_classifier != NULL) {
        return true;
    }

    if (dir == NULL) {
        dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;
    }

    char *classifier_path;

    char_array *path = char_array_new_size(strlen(dir) + PATH_SEPARATOR_LEN + strlen(LANGUAGE_CLASSIFIER_FILENAME));
    if (language_classifier == NULL) {
        char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, LANGUAGE_CLASSIFIER_FILENAME);
        classifier_path = char_array_get_string(path);

        language_classifier = language_classifier_load(classifier_path);

    }

    char_array_destroy(path);
    return true;
}

void language_classifier_module_teardown(void) {
    if (language_classifier != NULL) {
        language_classifier_destroy(language_classifier);
    }
    language_classifier = NULL;
}

