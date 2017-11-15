#include "averaged_perceptron.h"

#define PERCEPTRON_SIGNATURE 0xCBCBCBCB

static inline bool averaged_perceptron_get_feature_id(averaged_perceptron_t *self, char *feature, uint32_t *feature_id) {
    return trie_get_data(self->features, feature, feature_id);
}

inline double_array *averaged_perceptron_predict_scores(averaged_perceptron_t *self, cstring_array *features) {
    if (self->scores == NULL || self->scores->n == 0) self->scores = double_array_new_zeros((size_t)self->num_classes);

    double_array_zero(self->scores->a, self->scores->n);

    double *scores = self->scores->a;

    uint32_t i = 0;
    char *feature;
    uint32_t feature_id;

    uint32_t *indptr = self->weights->indptr->a;
    uint32_t *indices = self->weights->indices->a;
    double *data = self->weights->data->a;

    cstring_array_foreach(features, i, feature, {
        if (!averaged_perceptron_get_feature_id(self, feature, &feature_id)) {
            continue;
        }

        for (int col = indptr[feature_id]; col < indptr[feature_id + 1]; col++) {
            uint32_t class_id = indices[col];
            scores[class_id] += data[col];
        }
    })

    return self->scores;   
}

inline double_array *averaged_perceptron_predict_scores_counts(averaged_perceptron_t *self, khash_t(str_uint32) *feature_counts) {
    if (self->scores == NULL || self->scores->n == 0) self->scores = double_array_new_zeros((size_t)self->num_classes);

    double_array_zero(self->scores->a, self->scores->n);

    double *scores = self->scores->a;

    uint32_t i = 0;
    const char *feature;
    uint32_t count;
    uint32_t feature_id;

    uint32_t *indptr = self->weights->indptr->a;
    uint32_t *indices = self->weights->indices->a;
    double *data = self->weights->data->a;

    kh_foreach(feature_counts, feature, count, {
        if (!averaged_perceptron_get_feature_id(self, (char *)feature, &feature_id)) {
            continue;
        }

        for (int col = indptr[feature_id]; col < indptr[feature_id + 1]; col++) {
            uint32_t class_id = indices[col];
            scores[class_id] += data[col] * (double)count;
        }
    })

    return self->scores;
}


inline uint32_t averaged_perceptron_predict(averaged_perceptron_t *self, cstring_array *features) {
    double_array *scores = averaged_perceptron_predict_scores(self, features);

    int64_t max_score = double_array_argmax(scores->a, scores->n);

    return (uint32_t)max_score;

}

inline uint32_t averaged_perceptron_predict_counts(averaged_perceptron_t *self, khash_t(str_uint32) *feature_counts) {
    double_array *scores = averaged_perceptron_predict_scores_counts(self, feature_counts);

    int64_t max_score = double_array_argmax(scores->a, scores->n);

    return (uint32_t)max_score;
}

averaged_perceptron_t *averaged_perceptron_read(FILE *f) {
    if (f == NULL) return NULL;

    uint32_t signature;

    if (!file_read_uint32(f, &signature) || signature != PERCEPTRON_SIGNATURE) {
        return NULL;
    }

    averaged_perceptron_t *perceptron = calloc(1, sizeof(averaged_perceptron_t));

    if (!file_read_uint32(f, &perceptron->num_features) ||
        !file_read_uint32(f, &perceptron->num_classes) ||
        perceptron->num_classes == 0) {
        return NULL;
    }

    perceptron->weights = sparse_matrix_read(f);
    if (perceptron->weights == NULL) {
        goto exit_perceptron_created;
    }

    perceptron->scores = double_array_new_zeros((size_t)perceptron->num_classes);

    if (perceptron->scores == NULL) {
        goto exit_perceptron_created;
    }

    uint64_t classes_str_len;

    if (!file_read_uint64(f, &classes_str_len)) {
        goto exit_perceptron_created;
    }

    char_array *array = char_array_new_size(classes_str_len);

    if (array == NULL) {
        goto exit_perceptron_created;
    }

    if (!file_read_chars(f, array->a, classes_str_len)) {
        char_array_destroy(array);
        goto exit_perceptron_created;
    }

    array->n = classes_str_len;

    perceptron->classes = cstring_array_from_char_array(array);
    if (perceptron->classes == NULL) {
        goto exit_perceptron_created;
    }

    perceptron->features = trie_read(f);

    if (perceptron->features == NULL) {
        goto exit_perceptron_created;
    }

    return perceptron;

exit_perceptron_created:
    averaged_perceptron_destroy(perceptron);
    return NULL;
}

averaged_perceptron_t *averaged_perceptron_load(char *filename) {
    if (filename == NULL) return NULL;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return NULL;
    averaged_perceptron_t *perceptron = averaged_perceptron_read(f);
    fclose(f);
    return perceptron;
}

bool averaged_perceptron_write(averaged_perceptron_t *self, FILE *f) {
    if (self == NULL || f == NULL || self->weights == NULL || self->classes == NULL ||
        self->features == NULL) {
        return false;
    }

    if (!file_write_uint32(f, PERCEPTRON_SIGNATURE) ||
        !file_write_uint32(f, self->num_features) ||
        !file_write_uint32(f, self->num_classes)) {
        return false;
    }

    if (!sparse_matrix_write(self->weights, f)) {
        return false;
    }

    uint64_t classes_str_len = (uint64_t) cstring_array_used(self->classes);
    if (!file_write_uint64(f, classes_str_len)) {
        return false;
    }

    if (!file_write_chars(f, self->classes->str->a, classes_str_len)) {
        return false;
    }

    if (!trie_write(self->features, f)) {
        return false;
    }

    return true;
}

bool averaged_perceptron_save(averaged_perceptron_t *self, char *filename) {
    if (self == NULL || filename == NULL) return false;
    FILE *f = fopen(filename, "wb");
    if (f == NULL) return false;
    bool ret_val = averaged_perceptron_write(self, f);
    fclose(f);
    return ret_val;
}


void averaged_perceptron_destroy(averaged_perceptron_t *self) {
    if (self == NULL) return;

    if (self->features != NULL) {
        trie_destroy(self->features);
    }

    if (self->classes != NULL) {
        cstring_array_destroy(self->classes);
    }

    if (self->weights != NULL) {
        sparse_matrix_destroy(self->weights);
    }

    if (self->scores != NULL) {
        double_array_destroy(self->scores);
    }

    free(self);
}
