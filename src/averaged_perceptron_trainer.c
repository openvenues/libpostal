#include "averaged_perceptron_trainer.h"

void averaged_perceptron_trainer_destroy(averaged_perceptron_trainer_t *self) {
    if (self == NULL) return;

    const char *key;
    uint32_t id;

    if (self->features != NULL) {
        kh_foreach(self->features, key, id, {
            free((char *)key);
        })
        kh_destroy(str_uint32, self->features);
    }

    if (self->classes != NULL) {
        kh_foreach(self->classes, key, id, {
            free((char *)key);
        })
        kh_destroy(str_uint32, self->classes);
    }

    if (self->class_strings != NULL) {
        cstring_array_destroy(self->class_strings);
    }

    uint32_t feature_id;
    khash_t(class_weights) *weights;

    kh_foreach(self->weights, feature_id, weights, {
        kh_destroy(class_weights, weights);
    })

    if (self->weights != NULL) {
        kh_destroy(feature_class_weights, self->weights);
    }

    if (self->update_counts != NULL) {
        uint64_array_destroy(self->update_counts);
    }

    if (self->scores != NULL) {
        double_array_destroy(self->scores);
    }

    free(self);
}


bool averaged_perceptron_trainer_get_class_id(averaged_perceptron_trainer_t *self, char *class_name, uint32_t *class_id, bool add_if_missing) {
    khiter_t k;

    if (class_name == NULL) {
        log_error("class_name was NULL\n");
        return false;
    }

    khash_t(str_uint32) *classes = self->classes;

    k = kh_get(str_uint32, classes, class_name);
    if (k != kh_end(classes)) {
        *class_id = kh_value(classes, k);
        return true;
    } else if (add_if_missing) {
        uint32_t new_id = (uint32_t)kh_size(classes);
        int ret;
        char *key = strdup(class_name);
        if (key == NULL) {
            return false;
        }
        k = kh_put(str_uint32, classes, key, &ret);
        if (ret < 0) {
            return false;
        }
        kh_value(classes, k) = new_id;
        *class_id = new_id;

        cstring_array_add_string(self->class_strings, class_name);
        self->num_classes++;
        return true;
    }
    return false;
}

bool averaged_perceptron_trainer_get_feature_id(averaged_perceptron_trainer_t *self, char *feature, uint32_t *feature_id, bool add_if_missing) {
    khiter_t k;

    if (feature == NULL) {
        log_error("feature was NULL\n");
        return false;
    }

    khash_t(str_uint32) *features = self->features;


    k = kh_get(str_uint32, features, feature);
    if (k != kh_end(features)) {
        *feature_id = kh_value(features, k);
        return true;
    } else if (add_if_missing) {
        uint32_t new_id = (uint32_t)kh_size(features);
        int ret;
        char *key = strdup(feature);
        if (key == NULL) {
            return false;
        }
        k = kh_put(str_uint32, features, key, &ret);
        if (ret < 0) {
            return false;
        }
        kh_value(features, k) = new_id;
        *feature_id = new_id;

        uint64_array_push(self->update_counts, 0);
        self->num_features++;
        return true;
    }
    return false;

}

averaged_perceptron_t *averaged_perceptron_trainer_finalize(averaged_perceptron_trainer_t *self) {
    if (self == NULL || self->num_classes == 0) return NULL;

    uint32_t class_id;
    class_weight_t weight;

    uint64_t updates = self->num_updates;
    khash_t(class_weights) *weights;

    char **feature_keys = malloc(sizeof(char *) * self->num_features);
    uint32_t feature_id;
    const char *feature;
    kh_foreach(self->features, feature, feature_id, {
        if (feature_id >= self->num_features) {
            free(feature_keys);
            return NULL;
        }
        feature_keys[feature_id] = (char *)feature;
    })

    sparse_matrix_t *averaged_weights = sparse_matrix_new();

    uint32_t next_feature_id = 0;
    khiter_t k;

    uint64_t *update_counts = self->update_counts->a;

    log_info("Finalizing trainer, num_features=%u\n", self->num_features);

    log_info("Pruning weights with < min_updates = %" PRIu64 "\n", self->min_updates);

    for (feature_id = 0; feature_id < self->num_features; feature_id++) {
        k = kh_get(feature_class_weights, self->weights, feature_id);
        if (k == kh_end(self->weights)) {
            sparse_matrix_destroy(averaged_weights);
            free(feature_keys);
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
            k = kh_get(str_uint32, self->features, feature);
            if (k != kh_end(self->features)) {
                if (keep_feature) {
                    kh_value(self->features, k) = new_feature_id;
                } else {
                    kh_del(str_uint32, self->features, k);
                }
            } else {
                log_error("Error in kh_get on self->features\n");
                averaged_perceptron_trainer_destroy(self);
                return NULL;
            }
        }

    }

    free(feature_keys);

    self->num_features = kh_size(self->features);
    log_info("After pruning, num_features=%u\n", self->num_features);

    averaged_perceptron_t *perceptron = malloc(sizeof(averaged_perceptron_t));

    perceptron->weights = averaged_weights;

    trie_t *features = trie_new_from_hash(self->features);
    if (features == NULL) {
        averaged_perceptron_trainer_destroy(self);
        return NULL;
    }

    perceptron->features = features;

    perceptron->num_features = self->num_features;
    perceptron->num_classes = self->num_classes;

    perceptron->scores = double_array_new_zeros(perceptron->num_classes);

    // Set our pointers to NULL so they don't get free'd on destroy
    perceptron->classes = self->class_strings;
    self->class_strings = NULL;

    averaged_perceptron_trainer_destroy(self);

    return perceptron;
}

khash_t(class_weights) *averaged_perceptron_trainer_get_class_weights(averaged_perceptron_trainer_t *self, uint32_t feature_id, bool add_if_missing) {
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


static inline bool averaged_perceptron_trainer_update_weight(khash_t(class_weights) *weights, uint64_t iter, uint32_t class_id, double value) {
    class_weight_t weight = NULL_WEIGHT;
    size_t index;

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

static inline bool averaged_perceptron_trainer_update_feature(averaged_perceptron_trainer_t *self, uint32_t feature_id, uint32_t guess, uint32_t truth, double value) {
    bool add_if_missing = true;

    khash_t(class_weights) *weights = averaged_perceptron_trainer_get_class_weights(self, feature_id, add_if_missing);

    if (weights == NULL) {
        return false;
    }

    uint64_t updates = self->num_updates;

    if (!averaged_perceptron_trainer_update_weight(weights, updates, guess, -1.0 * value) ||
       !averaged_perceptron_trainer_update_weight(weights, updates, truth, value)) {
        return false;
    }

    uint64_t *update_counts = self->update_counts->a;
    update_counts[feature_id]++;

    return true;
}

uint32_t averaged_perceptron_trainer_predict(averaged_perceptron_trainer_t *self, cstring_array *features) {
    double_array *scores = self->scores;
    size_t num_classes = (size_t)self->num_classes;

    uint32_t i = 0;
    char *feature = NULL;
    bool add_if_missing = false;
    uint32_t feature_id;

    khash_t(class_weights) *weights;
    uint32_t class_id;
    class_weight_t weight;

    if (scores->m < num_classes) {
        double_array_resize(scores, num_classes);
    }

    if (scores->n < num_classes) {
        scores->n = num_classes;
    }

    double_array_zero(scores->a, scores->n);

    uint64_t *update_counts = self->update_counts->a;

    cstring_array_foreach(features, i, feature, {
        if (!averaged_perceptron_trainer_get_feature_id(self, feature, &feature_id, add_if_missing)) {
            continue;
        }

        uint64_t update_count = update_counts[feature_id];
        bool keep_feature = update_count >= self->min_updates;

        if (keep_feature) {
            weights = averaged_perceptron_trainer_get_class_weights(self, feature_id, add_if_missing);

            if (weights == NULL) {
                continue;
            }

            kh_foreach(weights, class_id, weight, {
                scores->a[class_id] += weight.value;
            })
        }
    })

    int64_t max_score = double_array_argmax(scores->a, scores->n);

    return (uint32_t)max_score;
}

bool averaged_perceptron_trainer_update(averaged_perceptron_trainer_t *self, uint32_t guess, uint32_t truth, cstring_array *features) {
    uint32_t i = 0;
    char *feature = NULL;
    uint32_t feature_id;
    bool add_if_missing = true;

    cstring_array_foreach(features, i, feature, {
        if (!averaged_perceptron_trainer_get_feature_id(self, feature, &feature_id, add_if_missing)) {
            return false;
        }

        if (!averaged_perceptron_trainer_update_feature(self, feature_id, guess, truth, 1.0)) {
            return false;
        }
    })

    self->num_updates++;

    return true;
}

bool averaged_perceptron_trainer_update_counts(averaged_perceptron_trainer_t *self, uint32_t guess, uint32_t truth, khash_t(str_uint32) *feature_counts) {
    const char *feature;
    uint32_t feature_id;
    uint32_t count;
    bool add_if_missing = true;

    kh_foreach(feature_counts, feature, count, {
        if (!averaged_perceptron_trainer_get_feature_id(self, (char *)feature, &feature_id, add_if_missing)) {
            return false;
        }

        if (!averaged_perceptron_trainer_update_feature(self, feature_id, guess, truth, (double)count)) {
            return false;
        }
    })

    self->num_updates++;

    return true;
}

bool averaged_perceptron_trainer_train_example(averaged_perceptron_trainer_t *self, void *tagger, void *context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *prev2_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, cstring_array *labels) {
    // Keep two tags of history in training
    char *prev = NULL;
    char *prev2 = NULL;

    uint32_t prev_id = 0;
    uint32_t prev2_id = 0;

    size_t num_tokens = tokenized->tokens->n;
    if (cstring_array_num_strings(labels) != num_tokens) {
        return false;
    }

    bool add_if_missing = true;

    for (uint32_t i = 0; i < num_tokens; i++) {
        cstring_array_clear(features);
        cstring_array_clear(prev_tag_features);
        cstring_array_clear(prev2_tag_features);

        char *label = cstring_array_get_string(labels, i);
        if (label == NULL) {
            log_error("label is NULL\n");
        }

        if (!feature_function(tagger, context, tokenized, i)) {
            log_error("Could not add address parser features\n");
            return false;
        }

        uint32_t truth;

        if (!averaged_perceptron_trainer_get_class_id(self, label, &truth, add_if_missing)) {
            log_error("Get class id failed\n");
            return false;
        }

        uint32_t fidx;
        const char *feature;

        if (i > 0) {
            prev = cstring_array_get_string(self->class_strings, prev_id);

            cstring_array_foreach(prev_tag_features, fidx, feature, {
                feature_array_add(features, 3, "prev", prev, (char *)feature);
            })

            if (i > 1) {
                prev2 = cstring_array_get_string(self->class_strings, prev2_id);
                cstring_array_foreach(prev2_tag_features, fidx, feature, {
                    feature_array_add(features, 5, "prev2", prev2, "prev", prev, (char *)feature);
                })
            }
        }

        uint32_t guess = averaged_perceptron_trainer_predict(self, features);

        // Online error-driven learning, only needs to update weights when it gets a wrong answer, making training fast
        if (guess != truth) {
            self->num_errors++;
            if (!averaged_perceptron_trainer_update(self, guess, truth, features)) {
                log_error("Trainer update failed\n");
                return false;
            }
        }

        prev2_id = prev_id;
        prev_id = guess;

    }

    return true;

}

averaged_perceptron_trainer_t *averaged_perceptron_trainer_new(uint64_t min_updates) {
    averaged_perceptron_trainer_t *self = calloc(1, sizeof(averaged_perceptron_trainer_t));

    if (self == NULL) return NULL;

    self->num_features = 0;
    self->num_classes = 0;
    self->num_updates = 0;
    self->num_errors = 0;
    self->iterations = 0;

    self->min_updates = min_updates;

    self->features = kh_init(str_uint32);
    if (self->features == NULL) {
        goto exit_trainer_created;
    }

    self->classes = kh_init(str_uint32);
    if (self->classes == NULL) {
        goto exit_trainer_created;
    }

    self->class_strings = cstring_array_new();
    if (self->class_strings == NULL) {
        goto exit_trainer_created;
    }

    self->weights = kh_init(feature_class_weights);

    if (self->weights == NULL) {
        goto exit_trainer_created;
    }

    self->update_counts = uint64_array_new();
    if (self->update_counts == NULL) {
        goto exit_trainer_created;
    }

    self->scores = double_array_new();
    if (self->scores == NULL) {
        goto exit_trainer_created;
    }

    return self;

exit_trainer_created:
    averaged_perceptron_trainer_destroy(self);
    return NULL;
}

