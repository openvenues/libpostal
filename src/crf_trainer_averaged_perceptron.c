#include "crf_trainer_averaged_perceptron.h"

void crf_averaged_perceptron_trainer_destroy(crf_averaged_perceptron_trainer_t *self) {
    if (self == NULL) return;

    uint32_t feature_id;
    khash_t(class_weights) *weights;

    if (self->weights != NULL) {
        kh_foreach(self->weights, feature_id, weights, {
            if (weights != NULL) {
                kh_destroy(class_weights, weights);
            }
        })
        kh_destroy(feature_class_weights, self->weights);
    }

    khash_t(prev_tag_class_weights) *prev_tag_weights;
    if (self->prev_tag_weights != NULL) {
        kh_foreach(self->prev_tag_weights, feature_id, prev_tag_weights, {
            if (prev_tag_weights != NULL) {
                kh_destroy(prev_tag_class_weights, prev_tag_weights);
            }
        })

        kh_destroy(feature_prev_tag_class_weights, self->prev_tag_weights);
    }

    if (self->trans_weights != NULL) {
        kh_destroy(prev_tag_class_weights, self->trans_weights);
    }

    if (self->update_counts != NULL) {
        uint64_array_destroy(self->update_counts);
    }

    if (self->prev_tag_update_counts != NULL) {
        uint64_array_destroy(self->prev_tag_update_counts);
    }

    if (self->sequence_features != NULL) {
        cstring_array_destroy(self->sequence_features);
    }

    if (self->sequence_features_indptr != NULL) {
        uint32_array_destroy(self->sequence_features_indptr);
    }

    if (self->sequence_prev_tag_features != NULL) {
        cstring_array_destroy(self->sequence_prev_tag_features);
    }

    if (self->sequence_prev_tag_features_indptr != NULL) {
        uint32_array_destroy(self->sequence_prev_tag_features_indptr);
    }

    if (self->label_ids != NULL) {
        uint32_array_destroy(self->label_ids);
    }

    if (self->viterbi != NULL) {
        uint32_array_destroy(self->viterbi);
    }

    if (self->base_trainer != NULL) {
        crf_trainer_destroy(self->base_trainer);
    }

    free(self);
}

crf_averaged_perceptron_trainer_t *crf_averaged_perceptron_trainer_new(size_t num_classes, size_t min_updates) {
    crf_averaged_perceptron_trainer_t *self = calloc(1, sizeof(crf_averaged_perceptron_trainer_t));

    if (self == NULL) return NULL;

    log_info("num_classes %zu\n", num_classes);

    self->num_updates = 0;
    self->num_errors = 0;
    self->iterations = 0;
    self->min_updates = min_updates;

    self->base_trainer = crf_trainer_new(num_classes);
    if (self->base_trainer == NULL) {
        goto exit_trainer_created;
    }

    self->weights = kh_init(feature_class_weights);

    if (self->weights == NULL) {
        goto exit_trainer_created;
    }

    self->prev_tag_weights = kh_init(feature_prev_tag_class_weights);

    if (self->prev_tag_weights == NULL) {
        goto exit_trainer_created;
    }

    self->trans_weights = kh_init(prev_tag_class_weights);
    if (self->trans_weights == NULL) {
        goto exit_trainer_created;
    }

    self->update_counts = uint64_array_new();
    if (self->update_counts == NULL) {
        goto exit_trainer_created;
    }

    self->prev_tag_update_counts = uint64_array_new();
    if (self->prev_tag_update_counts == NULL) {
        goto exit_trainer_created;
    }

    self->sequence_features = cstring_array_new();
    if (self->sequence_features == NULL) {
        goto exit_trainer_created;
    }

    self->sequence_features_indptr = uint32_array_new();
    if (self->sequence_features_indptr == NULL) {
        goto exit_trainer_created;
    }

    self->sequence_prev_tag_features = cstring_array_new();
    if (self->sequence_prev_tag_features == NULL) {
        goto exit_trainer_created;
    }

    self->sequence_prev_tag_features_indptr = uint32_array_new();
    if (self->sequence_prev_tag_features_indptr == NULL) {
        goto exit_trainer_created;
    }

    self->label_ids = uint32_array_new();
    if (self->label_ids == NULL) {
        goto exit_trainer_created;
    }

    self->viterbi = uint32_array_new();
    if (self->viterbi == NULL) {
        goto exit_trainer_created;
    }

    return self;

exit_trainer_created:
    crf_averaged_perceptron_trainer_destroy(self);
    return NULL;
}

static inline uint32_t tag_bigram_class_id(crf_averaged_perceptron_trainer_t *self, tag_bigram_t tag_bigram) {
    return tag_bigram.prev_class_id * self->base_trainer->num_classes + tag_bigram.class_id;
}

khash_t(class_weights) *crf_averaged_perceptron_trainer_get_class_weights(crf_averaged_perceptron_trainer_t *self, uint32_t feature_id, bool add_if_missing) {
    khiter_t k;
    k = kh_get(feature_class_weights, self->weights, feature_id);
    if (k != kh_end(self->weights)) {
        return kh_value(self->weights, k);
    } else if (add_if_missing) {
        khash_t(class_weights) *weights = kh_init(class_weights);
        int ret;
        k = kh_put(feature_class_weights, self->weights, feature_id, &ret);
        if (ret < 0) {
            kh_destroy(class_weights, weights);
            return NULL;
        }
        kh_value(self->weights, k) = weights;
        return weights;
    }

    return NULL;
}

khash_t(prev_tag_class_weights) *crf_averaged_perceptron_trainer_get_prev_tag_class_weights(crf_averaged_perceptron_trainer_t *self, uint32_t feature_id, bool add_if_missing) {
    khiter_t k;
    k = kh_get(feature_prev_tag_class_weights, self->prev_tag_weights, feature_id);
    if (k != kh_end(self->prev_tag_weights)) {
        return kh_value(self->prev_tag_weights, k);
    } else if (add_if_missing) {
        khash_t(prev_tag_class_weights) *weights = kh_init(prev_tag_class_weights);
        int ret;
        k = kh_put(feature_prev_tag_class_weights, self->prev_tag_weights, feature_id, &ret);
        if (ret < 0) {
            kh_destroy(prev_tag_class_weights, weights);
            return NULL;
        }
        kh_value(self->prev_tag_weights, k) = weights;
        return weights;
    }

    return NULL;
}


static inline bool crf_averaged_perceptron_trainer_update_weight(khash_t(class_weights) *weights, uint64_t iter, uint32_t class_id, double value) {
    class_weight_t weight = NULL_WEIGHT;

    khiter_t k;
    k = kh_get(class_weights, weights, class_id);
    if (k != kh_end(weights)) {
        weight = kh_value(weights, k);
    }

    weight.total += (iter - weight.last_updated) * weight.value;
    weight.last_updated = iter;
    weight.value += value;

    int ret;
    k = kh_put(class_weights, weights, class_id, &ret);
    if (ret < 0) return false;
    kh_value(weights, k) = weight;

    return true;

}

static inline bool crf_averaged_perceptron_trainer_update_prev_tag_weight(khash_t(prev_tag_class_weights) *weights, uint64_t iter, uint32_t prev_class_id, uint32_t class_id, double value) {
    class_weight_t weight = NULL_WEIGHT;

    tag_bigram_t tag_bigram;
    tag_bigram.prev_class_id = prev_class_id;
    tag_bigram.class_id = class_id;

    uint64_t key = tag_bigram.value;

    khiter_t k;
    k = kh_get(prev_tag_class_weights, weights, key);
    if (k != kh_end(weights)) {
        weight = kh_value(weights, k);
    }

    weight.total += (iter - weight.last_updated) * weight.value;
    weight.last_updated = iter;
    weight.value += value;

    int ret;
    k = kh_put(prev_tag_class_weights, weights, key, &ret);
    if (ret < 0) return false;
    kh_value(weights, k) = weight;

    return true;
}


static inline bool crf_averaged_perceptron_trainer_update_feature(crf_averaged_perceptron_trainer_t *self, uint32_t feature_id, uint32_t guess, uint32_t truth, double value) {
    bool add_if_missing = true;

    khash_t(class_weights) *weights = crf_averaged_perceptron_trainer_get_class_weights(self, feature_id, add_if_missing);

    if (weights == NULL) {
        return false;
    }

    uint64_t updates = self->num_updates;

    if (!crf_averaged_perceptron_trainer_update_weight(weights, updates, guess, -1.0 * value) ||
       !crf_averaged_perceptron_trainer_update_weight(weights, updates, truth, value)) {
        return false;
    }

    return true;
}


static inline bool crf_averaged_perceptron_trainer_update_prev_tag_feature(crf_averaged_perceptron_trainer_t *self, uint32_t feature_id, uint32_t prev_guess, uint32_t prev_truth, uint32_t guess, uint32_t truth, double value) {
    bool add_if_missing = true;
    khash_t(prev_tag_class_weights) *weights = crf_averaged_perceptron_trainer_get_prev_tag_class_weights(self, feature_id, add_if_missing);

    if (weights == NULL) {
        return false;
    }

    uint64_t updates = self->num_updates;

    if (!crf_averaged_perceptron_trainer_update_prev_tag_weight(weights, updates, prev_guess, guess, -1.0 * value) ||
       !crf_averaged_perceptron_trainer_update_prev_tag_weight(weights, updates, prev_truth, truth, value)) {
        return false;
    }

    return true;
}

static inline bool crf_averaged_perceptron_trainer_update_trans_feature(crf_averaged_perceptron_trainer_t *self, uint32_t prev_guess, uint32_t prev_truth, uint32_t guess, uint32_t truth, double value) {
    bool add_if_missing = true;
    khash_t(prev_tag_class_weights) *weights = self->trans_weights;

    if (weights == NULL) {
        return false;
    }

    uint64_t updates = self->num_updates;

    if (!crf_averaged_perceptron_trainer_update_prev_tag_weight(weights, updates, prev_guess, guess, -1.0 * value) ||
       !crf_averaged_perceptron_trainer_update_prev_tag_weight(weights, updates, prev_truth, truth, value)) {
        return false;
    }

    return true;
}


static inline bool crf_averaged_perceptron_trainer_cache_features(crf_averaged_perceptron_trainer_t *self, cstring_array *features) {
    size_t i;
    char *feature;
    uint32_t feature_id;

    cstring_array_foreach(features, i, feature, {
        cstring_array_add_string(self->sequence_features, feature);
    })

    size_t num_strings = cstring_array_num_strings(self->sequence_features);
    uint32_array_push(self->sequence_features_indptr, num_strings);
    return true;
}


static inline bool crf_averaged_perceptron_trainer_cache_prev_tag_features(crf_averaged_perceptron_trainer_t *self, cstring_array *features) {
    size_t i;
    char *feature;
    uint32_t feature_id;

    cstring_array_foreach(features, i, feature, {
        cstring_array_add_string(self->sequence_prev_tag_features, feature);
    })

    size_t num_strings = cstring_array_num_strings(self->sequence_prev_tag_features);
    uint32_array_push(self->sequence_prev_tag_features_indptr, num_strings);
    return true;
}


static bool crf_averaged_perceptron_trainer_state_score(crf_averaged_perceptron_trainer_t *self) {
    if (self == NULL || self->base_trainer == NULL ||
        self->sequence_features == NULL || self->sequence_features_indptr == NULL) {
        return false;
    }
    crf_context_t *context = self->base_trainer->context;

    uint32_t class_id;

    class_weight_t weight;

    cstring_array *sequence_features = self->sequence_features;

    uint64_t *update_counts = self->update_counts->a;

    size_t num_tokens = self->sequence_features_indptr->n - 1;
    uint32_t *indptr = self->sequence_features_indptr->a;

    for (size_t t = 0; t < num_tokens; t++) {
        uint32_t idx = indptr[t];
        uint32_t next_start = indptr[t + 1];

        double *scores = state_score(context, t);

        for (uint32_t j = idx; j < next_start; j++) {
            char *feature = cstring_array_get_string(sequence_features, j);

            uint32_t feature_id;
            if (!crf_trainer_get_feature_id(self->base_trainer, feature, &feature_id)) {
                continue;
            }
            uint64_t update_count = update_counts[feature_id];
            bool keep_feature = update_count >= self->min_updates;

            if (keep_feature) {
                bool add_if_missing = false;
                khash_t(class_weights) *weights = crf_averaged_perceptron_trainer_get_class_weights(self, feature_id, add_if_missing);

                if (weights == NULL) {
                    continue;
                }

                kh_foreach(weights, class_id, weight, {
                    scores[class_id] += weight.value;
                })
            }
        }
    }

    return true;
}

static bool crf_averaged_perceptron_trainer_state_trans_score(crf_averaged_perceptron_trainer_t *self) {
    if (self == NULL || self->base_trainer == NULL ||
        self->sequence_prev_tag_features == NULL || self->sequence_features_indptr == NULL) {
        return false;
    }
    crf_context_t* context = self->base_trainer->context;

    uint32_t t = 0;
    uint32_t idx = 0;
    uint32_t length = 0;

    bool add_if_missing = false;

    class_weight_t weight;

    cstring_array *sequence_features = self->sequence_prev_tag_features;
    uint64_t *update_counts = self->prev_tag_update_counts->a;

    size_t num_tokens = self->sequence_prev_tag_features_indptr->n - 1;
    uint32_t *indptr = self->sequence_prev_tag_features_indptr->a;

    for (size_t t = 0; t < num_tokens; t++) {
        uint32_t idx = indptr[t];
        uint32_t next_start = indptr[t + 1];

        double *scores = state_trans_score_all(context, t);

        for (uint32_t j = idx; j < next_start; j++) {
            char *feature = cstring_array_get_string(sequence_features, j);

            uint32_t feature_id;
            if (!crf_trainer_get_prev_tag_feature_id(self->base_trainer, feature, &feature_id)) {
                continue;
            }
            uint64_t update_count = update_counts[feature_id];
            bool keep_feature = update_count >= self->min_updates;

            if (keep_feature) {
                bool add_if_missing = false;
                khash_t(prev_tag_class_weights) *prev_tag_weights = crf_averaged_perceptron_trainer_get_prev_tag_class_weights(self, feature_id, add_if_missing);

                if (prev_tag_weights == NULL) {
                    continue;
                }

                tag_bigram_t tag_bigram;
                uint64_t tag_bigram_key;

                kh_foreach(prev_tag_weights, tag_bigram_key, weight, {
                    tag_bigram.value = tag_bigram_key;
                    uint32_t class_id = tag_bigram_class_id(self, tag_bigram);
                    scores[class_id] += weight.value;
                })
            }
        }
    }

    return true;
}

static bool crf_averaged_perceptron_trainer_trans_score(crf_averaged_perceptron_trainer_t *self) {
    if (self == NULL || self->base_trainer == NULL || self->trans_weights == NULL) return false;
    crf_context_t *context = self->base_trainer->context;

    khash_t(prev_tag_class_weights) *trans_weights = self->trans_weights;

    class_weight_t weight;
    tag_bigram_t tag_bigram;
    uint64_t tag_bigram_key;

    double *scores = context->trans->values;

    kh_foreach(trans_weights, tag_bigram_key, weight, {
        tag_bigram.value = tag_bigram_key;
        uint32_t class_id = tag_bigram_class_id(self, tag_bigram);
        scores[class_id] += weight.value;
    })

    return true;
}

bool crf_averaged_perceptron_trainer_update(crf_averaged_perceptron_trainer_t *self, double value) {
    if (self->viterbi == NULL || self->label_ids == NULL || self->label_ids->n != self->viterbi->n ||
        self->sequence_features == NULL || self->sequence_features_indptr == NULL ||
        self->label_ids->n != self->sequence_features_indptr->n - 1 ||
        self->sequence_prev_tag_features == NULL || self->sequence_prev_tag_features_indptr == NULL ||
        self->label_ids->n != self->sequence_prev_tag_features_indptr->n - 1 ||
        self->update_counts == NULL || self->prev_tag_update_counts == NULL) {
        log_error("Something was NULL\n");
        return false;
    }

    uint32_t t, idx, length;

    bool add_if_missing = false;

    uint32_t *viterbi = self->viterbi->a;
    uint32_t *labels = self->label_ids->a;

    uint32_t truth, guess;

    size_t num_tokens = self->sequence_features_indptr->n - 1;
    uint32_t *indptr = self->sequence_features_indptr->a;

    cstring_array *sequence_features = self->sequence_features;

    for (size_t t = 0; t < num_tokens; t++) {
        truth = labels[t];
        guess = viterbi[t];

        if (guess != truth) {
            uint32_t idx = indptr[t];
            uint32_t next_start = indptr[t + 1];

            for (uint32_t j = idx; j < next_start; j++) {
                char *feature = cstring_array_get_string(sequence_features, j);
                if (feature == NULL) {
                    log_error("feature NULL, j = %u, len = %zu\n", j, cstring_array_num_strings(sequence_features));
                    return false;
                }

                uint32_t feature_id;
                bool exists;
                if (!crf_trainer_hash_feature_to_id_exists(self->base_trainer, feature, &feature_id, &exists)) {
                    return false;
                }

                if (!crf_averaged_perceptron_trainer_update_feature(self, feature_id, guess, truth, value)) {
                    return false;
                }

                if (exists) {
                    self->update_counts->a[feature_id]++;
                } else {
                    uint64_array_push(self->update_counts, 1);
                }
            }
            // This is shared between the state and state-trans features, only increment once
            self->num_updates++;
            self->num_errors++;
        }
    }

    uint32_t prev_truth, prev_guess;

    uint64_t *prev_tag_update_counts = self->prev_tag_update_counts->a;

    sequence_features = self->sequence_prev_tag_features;

    num_tokens = self->sequence_prev_tag_features_indptr->n - 1;
    indptr = self->sequence_prev_tag_features_indptr->a;

    for (size_t t = 0; t < num_tokens; t++) {
        truth = labels[t];
        guess = viterbi[t];

        if (t > 0 && (guess != truth || prev_guess != prev_truth)) {
            uint32_t idx = indptr[t];
            uint32_t next_start = indptr[t + 1];

            for (uint32_t j = idx; j < next_start; j++) {
                char *feature = cstring_array_get_string(sequence_features, j);

                if (feature == NULL) {
                    log_error("feature NULL, j = %u, len = %zu\n", j, cstring_array_num_strings(sequence_features));
                    return false;
                }

                uint32_t feature_id;
                bool exists;
                if (!crf_trainer_hash_prev_tag_feature_to_id_exists(self->base_trainer, feature, &feature_id, &exists)) {
                    return false;
                }

                if (!crf_averaged_perceptron_trainer_update_prev_tag_feature(self, feature_id, prev_guess, prev_truth, guess, truth, value)) {
                    return false;
                }

                if (exists) {
                    self->prev_tag_update_counts->a[feature_id]++;
                } else {
                    uint64_array_push(self->prev_tag_update_counts, 1);
                }
            }

        }

        prev_truth = truth;
        prev_guess = guess;
    }

    size_t sequence_len = self->label_ids->n;

    for (t = 0; t < sequence_len; t++) {
        truth = labels[t];
        guess = viterbi[t];

        if (t > 0 && (guess != truth || prev_guess != prev_truth)) {
            if (!crf_averaged_perceptron_trainer_update_trans_feature(self, prev_guess, prev_truth, guess, truth, value)) {
                return false;
            }
        }

        prev_truth = truth;
        prev_guess = guess;
    }

    return true;
}


bool crf_averaged_perceptron_trainer_train_example(crf_averaged_perceptron_trainer_t *self, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, cstring_array *labels) {
    if (self == NULL || self->base_trainer == NULL) return false;

    size_t num_tokens = tokenized->tokens->n;
    if (cstring_array_num_strings(labels) != num_tokens) {
        return false;
    }

    if (num_tokens == 0) {
        return true;
    }

    uint32_array_clear(self->sequence_features_indptr);
    uint32_array_push(self->sequence_features_indptr, 0);
    cstring_array_clear(self->sequence_features);

    uint32_array_clear(self->sequence_prev_tag_features_indptr);
    uint32_array_push(self->sequence_prev_tag_features_indptr, 0);
    cstring_array_clear(self->sequence_prev_tag_features);

    crf_context_t *crf_context = self->base_trainer->context;

    if (!uint32_array_resize(self->label_ids, num_tokens)) {
        log_error("Resizing label_ids failed\n");
        return false;
    }
    uint32_array_clear(self->label_ids);

    if (!crf_context_set_num_items(crf_context, num_tokens)) {
        return false;
    }

    crf_context_reset(crf_context, CRF_CONTEXT_RESET_ALL);

    bool add_if_missing = true;

    for (uint32_t i = 0; i < num_tokens; i++) {
        cstring_array_clear(features);
        cstring_array_clear(prev_tag_features);

        if (!feature_function(tagger, tagger_context, tokenized, i)) {
            log_error("Could not add address parser features\n");
            return false;
        }

        char *label = cstring_array_get_string(labels, i);
        if (label == NULL) {
            log_error("label is NULL\n");
        }

        uint32_t class_id;

        if (!crf_trainer_get_class_id(self->base_trainer, label, &class_id, add_if_missing)) {
            log_error("Get class id failed\n");
            return false;
        }

        uint32_array_push(self->label_ids, class_id);

        if (!crf_averaged_perceptron_trainer_cache_features(self, features) ||
            !crf_averaged_perceptron_trainer_cache_prev_tag_features(self, prev_tag_features)) {
            log_error("Caching features failed\n");
            return false;
        }
    }

    if (!crf_averaged_perceptron_trainer_state_score(self)) {
        log_error("Error in state score\n");
        return false;
    }
    
    if (!crf_averaged_perceptron_trainer_state_trans_score(self)) {
        log_error("Error in state_trans score\n");
        return false;
    }

    if (!crf_averaged_perceptron_trainer_trans_score(self)) {
        log_error("Error in trans score\n");
        return false;
    }

    if (!uint32_array_resize_fixed(self->viterbi, num_tokens)) {
        log_error("Error resizing Viterbi, num_tokens=%zu\n", num_tokens);
        return false;
    }

    uint32_t *viterbi = self->viterbi->a;
    double viterbi_score = crf_context_viterbi(crf_context, viterbi);

    if (self->viterbi->n != num_tokens || self->label_ids->n != num_tokens) {
        log_error("self->viterbi->n=%zu, num_tokens=%zu, self->label_ids->n=%zu\n", self->viterbi->n, num_tokens, self->label_ids->n);
        return false;
    }

    uint32_t *true_labels = self->label_ids->a;


    for (uint32_t i = 0; i < num_tokens; i++) {
        uint32_t truth = true_labels[i];

        // Technically this is supposed to be updated all at once
        uint32_t guess = viterbi[i];

        if (guess != truth) {
            if (!crf_averaged_perceptron_trainer_update(self, 1.0)) {
                log_error("Error in crf_averaged_perceptron_trainer_update\n");
                return false;
            }
            break;
        }

    }

    return true;
}


crf_t *crf_averaged_perceptron_trainer_finalize(crf_averaged_perceptron_trainer_t *self) {
    if (self == NULL || self->base_trainer == NULL || self->base_trainer->num_classes == 0) {
        log_error("Something was NULL\n");
        return NULL;
    }

    uint32_t class_id;
    class_weight_t weight;

    khiter_t k;

    size_t num_features = kh_size(self->base_trainer->features);

    sparse_matrix_t *averaged_weights = sparse_matrix_new();
    if (averaged_weights == NULL) {
        log_error("Error creating averaged_weights\n");
        return NULL;
    }

    log_info("Finalizing trainer, num_features=%zu\n", num_features);

    char **feature_keys = malloc(sizeof(char *) * num_features);
    uint32_t feature_id;
    const char *feature;

    kh_foreach(self->base_trainer->features, feature, feature_id, {
        if (feature_id >= num_features) {
            free(feature_keys);
            log_error("Error populating feature_keys, feature_id=%u, num_features=%zu\n", feature_id, num_features);
            return NULL;
        }
        feature_keys[feature_id] = (char *)feature;
    })

    khash_t(str_uint32) *features = self->base_trainer->features;
    khash_t(str_uint32) *prev_tag_features = self->base_trainer->prev_tag_features;

    uint64_t updates = self->num_updates;
    khash_t(class_weights) *weights;

    uint32_t next_feature_id = 0;
    uint64_t *update_counts = self->update_counts->a;

    log_info("Pruning weights with < min_updates = %" PRIu64 "\n", self->min_updates);

    for (feature_id = 0; feature_id < num_features; feature_id++) {
        k = kh_get(feature_class_weights, self->weights, feature_id);
        if (k == kh_end(self->weights)) {
            sparse_matrix_destroy(averaged_weights);
            free(feature_keys);
            log_error("Error in kh_get on self->weights, feature_id=%u, num_features=%zu\n", feature_id, num_features);
            return NULL;
        }

        weights = kh_value(self->weights, k);
        uint32_t class_id;

        uint64_t update_count = update_counts[feature_id];
        bool keep_feature = update_count >= self->min_updates;

        uint32_t new_feature_id = next_feature_id;

        if (keep_feature) {
            kh_foreach(weights, class_id, weight, {
                weight.total += (updates - weight.last_updated) * weight.value;
                double value = weight.total / updates;
                sparse_matrix_append(averaged_weights, class_id, value);
            })

            sparse_matrix_finalize_row(averaged_weights);
            next_feature_id++;
        }


        if (!keep_feature || new_feature_id != feature_id) {
            feature = feature_keys[feature_id];
            k = kh_get(str_uint32, features, feature);
            if (k != kh_end(features)) {
                if (keep_feature) {
                    kh_value(features, k) = new_feature_id;
                } else {
                    kh_del(str_uint32, features, k);
                }
            } else {
                log_error("Error in kh_get on features\n");
                crf_averaged_perceptron_trainer_destroy(self);
                free(feature_keys);
                return NULL;
            }
        }

    }

    free(feature_keys);

    num_features = kh_size(features);
    log_info("After pruning, num_features=%zu\n", num_features);

    sparse_matrix_t *averaged_state_trans_weights = sparse_matrix_new();
    if (averaged_state_trans_weights == NULL) {
        log_error("Error creating averaged_state_trans_weights\n");
        return NULL;
    }

    size_t num_prev_tag_features = kh_size(prev_tag_features);

    char **prev_tag_feature_keys = malloc(sizeof(char *) * num_prev_tag_features);


    kh_foreach(prev_tag_features, feature, feature_id, {
        if (feature_id >= num_prev_tag_features) {
            free(prev_tag_feature_keys);
            log_error("Error populating prev_tag_feature_keys\n");
            return NULL;
        }
        prev_tag_feature_keys[feature_id] = (char *)feature;
    })

    khash_t(prev_tag_class_weights) *prev_tag_weights;

    log_info("Pruning previous tag features, num_prev_tag_features=%zu\n", num_prev_tag_features);

    uint32_t next_prev_tag_feature_id = 0;

    uint64_t *prev_tag_update_counts = self->prev_tag_update_counts->a;

    tag_bigram_t tag_bigram;
    uint64_t tag_bigram_key;

    for (feature_id = 0; feature_id < num_prev_tag_features; feature_id++) {
        k = kh_get(feature_prev_tag_class_weights, self->prev_tag_weights, feature_id);
        if (k == kh_end(self->prev_tag_weights)) {
            sparse_matrix_destroy(averaged_state_trans_weights);
            free(prev_tag_feature_keys);
            log_error("Error in kh_get self->prev_tag_weights\n");
            return NULL;
        }

        prev_tag_weights = kh_value(self->prev_tag_weights, k);

        uint64_t update_count = prev_tag_update_counts[feature_id];
        bool keep_feature = update_count >= self->min_updates;

        uint32_t new_feature_id = next_prev_tag_feature_id;

        if (keep_feature) {
            kh_foreach(prev_tag_weights, tag_bigram_key, weight, {
                tag_bigram.value = tag_bigram_key;
                weight.total += (updates - weight.last_updated) * weight.value;
                double value = weight.total / updates;
                class_id = tag_bigram_class_id(self, tag_bigram);
                sparse_matrix_append(averaged_state_trans_weights, class_id, value);
            })

            sparse_matrix_finalize_row(averaged_state_trans_weights);

            next_prev_tag_feature_id++;
        }

        if (!keep_feature || new_feature_id != feature_id) {
            feature = prev_tag_feature_keys[feature_id];
            k = kh_get(str_uint32, prev_tag_features, feature);
            if (k != kh_end(prev_tag_features)) {
                if (keep_feature) {
                    kh_value(prev_tag_features, k) = new_feature_id;
                } else {
                    kh_del(str_uint32, prev_tag_features, k);
                }
            } else {
                log_error("Error in kh_get on prev_tag_features\n");
                crf_averaged_perceptron_trainer_destroy(self);
                free(prev_tag_feature_keys);
                return NULL;
            }
        }

    }

    free(prev_tag_feature_keys);

    num_prev_tag_features = kh_size(prev_tag_features);
    log_info("After pruning, num_prev_tag_features=%zu\n", num_prev_tag_features);


    size_t num_classes = self->base_trainer->num_classes;

    double_matrix_t *averaged_trans_weights = double_matrix_new_zeros(num_classes, num_classes);
    if (averaged_trans_weights == NULL) {
        log_error("Error creating double matrix for transition weights\n");
        return NULL;
    }

    double *trans = averaged_trans_weights->values;

    kh_foreach(self->trans_weights, tag_bigram_key, weight, {
        tag_bigram.value = tag_bigram_key;
        weight.total += (updates - weight.last_updated) * weight.value;
        double value = weight.total / updates;
        class_id = tag_bigram_class_id(self, tag_bigram);
        trans[class_id] = value;
    })

    crf_t *crf = malloc(sizeof(crf_t));

    crf->num_classes = num_classes;
    crf->weights = averaged_weights;
    crf->state_trans_weights = averaged_state_trans_weights;
    crf->trans_weights = averaged_trans_weights;
    crf->classes = self->base_trainer->class_strings;
    self->base_trainer->class_strings = NULL;

    trie_t *state_features = trie_new_from_hash(features);
    if (state_features == NULL) {
        crf_averaged_perceptron_trainer_destroy(self);
        log_error("Error creating state_features\n");
        return NULL;
    }

    crf->state_features = state_features;

    trie_t *state_trans_features = trie_new_from_hash(prev_tag_features);
    if (state_trans_features == NULL) {
        crf_averaged_perceptron_trainer_destroy(self);
        log_error("Error creating state_trans_features\n");
        return NULL;
    }

    crf->state_trans_features = state_trans_features;

    crf->viterbi = uint32_array_new();

    crf->context = crf_context_new(CRF_CONTEXT_VITERBI | CRF_CONTEXT_MARGINALS, num_classes, CRF_CONTEXT_DEFAULT_NUM_ITEMS);

    crf_averaged_perceptron_trainer_destroy(self);

    return crf;
}
