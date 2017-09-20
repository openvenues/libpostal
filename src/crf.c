#include "crf.h"
#include "log/log.h"

#define CRF_SIGNATURE 0xCFCFCFCF


static inline bool crf_get_feature_id(crf_t *self, char *feature, uint32_t *feature_id) {
    return trie_get_data(self->state_features, feature, feature_id);
}

static inline bool crf_get_state_trans_feature_id(crf_t *self, char *feature, uint32_t *feature_id) {
    return trie_get_data(self->state_trans_features, feature, feature_id);
}

bool crf_tagger_score(crf_t *self, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features) {
    if (self == NULL || feature_function == NULL || tokenized == NULL ) {
        return false;
    }
    size_t num_tokens = tokenized->tokens->n;

    crf_context_t *crf_context = self->context;
    crf_context_set_num_items(crf_context, num_tokens);
    crf_context_reset(crf_context, CRF_CONTEXT_RESET_ALL);

    if (!double_matrix_copy(self->trans_weights, crf_context->trans)) {
        return false;
    }

    for (uint32_t t = 0; t < num_tokens; t++) {
        cstring_array_clear(features);
        cstring_array_clear(prev_tag_features);

        if (!feature_function(tagger, tagger_context, tokenized, t)) {
            log_error("Could not add address parser features\n");
            return false;
        }

        uint32_t fidx;
        char *feature;

        if (print_features) {
            printf("{ ");
            size_t num_features = cstring_array_num_strings(features);
            cstring_array_foreach(features, fidx, feature, {
                printf("%s", feature);
                if (fidx < num_features - 1) printf(", ");
            })
            size_t num_prev_tag_features = cstring_array_num_strings(prev_tag_features);
            if (num_prev_tag_features > 0) {
                printf(", ");
            }
            cstring_array_foreach(prev_tag_features, fidx, feature, {
                printf("prev tag+%s", feature);
                if (fidx < num_prev_tag_features - 1) printf(", ");
            })
            printf(" }\n");
        }

        uint32_t feature_id;

        double *state_scores = state_score(crf_context, t);

        uint32_t *indptr = self->weights->indptr->a;
        uint32_t *indices = self->weights->indices->a;
        double *data = self->weights->data->a;

        cstring_array_foreach(features, fidx, feature, {
            if (!crf_get_feature_id(self, feature, &feature_id)) {
                continue;
            }

            for (int col = indptr[feature_id]; col < indptr[feature_id + 1]; col++) {
                uint32_t class_id = indices[col];
                state_scores[class_id] += data[col];
            }
        })

        double *state_trans_scores = state_trans_score_all(crf_context, t);

        indptr = self->state_trans_weights->indptr->a;
        indices = self->state_trans_weights->indices->a;
        data = self->state_trans_weights->data->a;

        cstring_array_foreach(prev_tag_features, fidx, feature, {
            if (!crf_get_state_trans_feature_id(self, feature, &feature_id)) {
                continue;
            }

            for (int col = indptr[feature_id]; col < indptr[feature_id + 1]; col++) {
                // Note: here there are L * L classes
                uint32_t class_id = indices[col];
                state_trans_scores[class_id] += data[col];
            }
        })

    }
    return true;
}

bool crf_tagger_score_viterbi(crf_t *self, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, double *score, bool print_features) {
    if (!crf_tagger_score(self, tagger, tagger_context, features, prev_tag_features, feature_function, tokenized, print_features)) {
        return false;
    }

    size_t num_tokens = tokenized->tokens->n;

    uint32_array_resize_fixed(self->viterbi, num_tokens);
    double viterbi_score = crf_context_viterbi(self->context, self->viterbi->a);

    *score = viterbi_score;

    return true;
}


bool crf_tagger_predict(crf_t *self, void *tagger, void *context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *labels, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features) {
    double score;

    if (labels == NULL) return false;
    if (!crf_tagger_score_viterbi(self, tagger, context, features, prev_tag_features, feature_function, tokenized, &score, print_features)) {
        return false;
    }

    uint32_t *viterbi = self->viterbi->a;

    for (size_t i = 0; i < self->viterbi->n; i++) {
        char *predicted = cstring_array_get_string(self->classes, viterbi[i]);
        cstring_array_add_string(labels, predicted);
    }

    return true;
}


bool crf_write(crf_t *self, FILE *f) {
    if (self == NULL || f == NULL || self->weights == NULL || self->classes == NULL ||
        self->state_features == NULL || self->state_trans_features == NULL) {
        log_info("something was NULL\n");
        return false;
    }

    if (!file_write_uint32(f, CRF_SIGNATURE) ||
        !file_write_uint32(f, self->num_classes)) {
        log_info("error writing header\n");
        return false;
    }

    uint64_t classes_str_len = (uint64_t) cstring_array_used(self->classes);
    if (!file_write_uint64(f, classes_str_len)) {
        log_info("error writing classes_str_len\n");
        return false;
    }

    if (!file_write_chars(f, self->classes->str->a, classes_str_len)) {
        log_info("error writing chars\n");
        return false;
    }

    if (!trie_write(self->state_features, f)) {
        log_info("error state_features\n");
        return false;
    }

    if (!sparse_matrix_write(self->weights, f)) {
        log_info("error weights\n");
        return false;
    }

    if (!trie_write(self->state_trans_features, f)) {
        log_info("error state_trans_features\n");
        return false;
    }

    if (!sparse_matrix_write(self->state_trans_weights, f)) {
        log_info("error state_trans_weights\n");
        return false;
    }

    if (!double_matrix_write(self->trans_weights, f)) {
        log_info("error trans_weights\n");
        return false;
    }

    return true;
}


bool crf_save(crf_t *self, char *filename) {
    if (self == NULL || filename == NULL) {
        log_info("crf or filename was NULL\n");
        return false;
    }
    FILE *f = fopen(filename, "wb");
    if (f == NULL) return false;
    bool ret_val = crf_write(self, f);
    fclose(f);
    return ret_val;
}


crf_t *crf_read(FILE *f) {
    if (f == NULL) return NULL;

    uint32_t signature;

    if (!file_read_uint32(f, &signature) || signature != CRF_SIGNATURE) {
        return NULL;
    }

    crf_t *crf = calloc(1, sizeof(crf_t));
    if (crf == NULL) return NULL;

    if (!file_read_uint32(f, &crf->num_classes) ||
        crf->num_classes == 0) {
        free(crf);
        return NULL;
    }

    uint64_t classes_str_len;

    if (!file_read_uint64(f, &classes_str_len)) {
        goto exit_crf_created;
    }

    char_array *array = char_array_new_size(classes_str_len);

    if (array == NULL) {
        goto exit_crf_created;
    }

    if (!file_read_chars(f, array->a, classes_str_len)) {
        char_array_destroy(array);
        goto exit_crf_created;
    }

    array->n = classes_str_len;

    crf->classes = cstring_array_from_char_array(array);
    if (crf->classes == NULL) {
        goto exit_crf_created;
    }

    crf->state_features = trie_read(f);
    if (crf->state_features == NULL) {
        goto exit_crf_created;
    }

    crf->weights = sparse_matrix_read(f);
    if (crf->weights == NULL) {
        goto exit_crf_created;
    }

    crf->state_trans_features = trie_read(f);
    if (crf->state_trans_features == NULL) {
        goto exit_crf_created;
    }

    crf->state_trans_weights = sparse_matrix_read(f);
    if (crf->state_trans_weights == NULL) {
        goto exit_crf_created;
    }

    crf->trans_weights = double_matrix_read(f);
    if (crf->trans_weights == NULL) {
        goto exit_crf_created;
    }

    crf->viterbi = uint32_array_new();
    if (crf->viterbi == NULL) {
        goto exit_crf_created;
    }

    crf->context = crf_context_new(CRF_CONTEXT_VITERBI | CRF_CONTEXT_MARGINALS, crf->num_classes, CRF_CONTEXT_DEFAULT_NUM_ITEMS);
    if (crf->context == NULL) {
        goto exit_crf_created;
    }

    return crf;

exit_crf_created:
    crf_destroy(crf);
    return NULL;
}

crf_t *crf_load(char *filename) {
    if (filename == NULL) return NULL;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) return NULL;
    crf_t *crf = crf_read(f);
    fclose(f);
    return crf;
}

void crf_destroy(crf_t *self) {
    if (self == NULL) return;

    if (self->classes != NULL) {
        cstring_array_destroy(self->classes);
    }

    if (self->state_features != NULL) {
        trie_destroy(self->state_features);
    }

    if (self->weights != NULL) {
        sparse_matrix_destroy(self->weights);
    }

    if (self->state_trans_features != NULL) {
        trie_destroy(self->state_trans_features);
    }

    if (self->state_trans_weights != NULL) {
        sparse_matrix_destroy(self->state_trans_weights);
    }

    if (self->trans_weights != NULL) {
        double_matrix_destroy(self->trans_weights);
    }

    if (self->viterbi != NULL) {
        uint32_array_destroy(self->viterbi);
    }

    if (self->context != NULL) {
        crf_context_destroy(self->context);
    }

    free(self);
}
