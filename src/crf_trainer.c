#include "crf_trainer.h"

void crf_trainer_destroy(crf_trainer_t *self) {
    if (self == NULL) return;

    const char *key;
    uint32_t id;

    if (self->features != NULL) {
        kh_foreach(self->features, key, id, {
            free((char *)key);
        })
        kh_destroy(str_uint32, self->features);
    }

    if (self->prev_tag_features != NULL) {
        kh_foreach(self->prev_tag_features, key, id, {
            free((char *)key);
        })
        kh_destroy(str_uint32, self->prev_tag_features);
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

    if (self->context != NULL) {
        crf_context_destroy(self->context);
    }

    free(self);
}

crf_trainer_t *crf_trainer_new(size_t num_classes)  {
    crf_trainer_t *trainer = malloc(sizeof(crf_trainer_t));
    if (trainer == NULL) return NULL;

    trainer->num_classes = num_classes;

    trainer->features = kh_init(str_uint32);
    if (trainer->features == NULL) {
        goto exit_trainer_created;
    }

    trainer->prev_tag_features = kh_init(str_uint32);
    if (trainer->prev_tag_features == NULL) {
        goto exit_trainer_created;
    }

    trainer->classes = kh_init(str_uint32);
    if (trainer->classes == NULL) {
        goto exit_trainer_created;
    }

    trainer->class_strings = cstring_array_new();
    if (trainer->class_strings == NULL) {
        goto exit_trainer_created;
    }
    
    trainer->context = crf_context_new(CRF_CONTEXT_VITERBI | CRF_CONTEXT_MARGINALS, num_classes, CRF_CONTEXT_DEFAULT_NUM_ITEMS);
    if (trainer->context == NULL) {
        goto exit_trainer_created;
    }

    return trainer;

exit_trainer_created:
    crf_trainer_destroy(trainer);
    return NULL;

}

bool crf_trainer_get_class_id_exists(crf_trainer_t *self, char *class_name, uint32_t *class_id, bool add_if_missing, bool *exists) {
    khiter_t k;

    if (class_name == NULL) {
        log_error("class_name was NULL\n");
        return false;
    }

    khash_t(str_uint32) *classes = self->classes;

    k = kh_get(str_uint32, classes, class_name);
    if (k != kh_end(classes)) {
        *class_id = kh_value(classes, k);
        *exists = true;
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
        *exists = false;
        cstring_array_add_string(self->class_strings, class_name);
        return true;
    }
    return false;
}

inline bool crf_trainer_get_class_id(crf_trainer_t *self, char *class_name, uint32_t *class_id, bool add_if_missing) {
    bool exists;
    return crf_trainer_get_class_id_exists(self, class_name, class_id, add_if_missing, &exists);
}

crf_trainer_t *crf_trainer_new_classes(cstring_array *classes)  {
    if (classes == NULL) return NULL;
    size_t num_classes = cstring_array_num_strings(classes);
    crf_trainer_t *trainer = crf_trainer_new(num_classes);
    if (trainer == NULL) return NULL;

    size_t i;
    char *class_name;
    uint32_t class_id;
    bool add_if_missing = true;

    cstring_array_foreach(classes, i, class_name, {
        bool exists;
        if (!crf_trainer_get_class_id_exists(trainer, class_name, &class_id, add_if_missing, &exists)) {
            crf_trainer_destroy(trainer);
            return NULL;
        }

        if (exists) {
            log_error("Duplicate class: %s\n", class_name);
            crf_trainer_destroy(trainer);
            return NULL;
        }
    })
    return trainer;
}

bool crf_trainer_hash_to_id(khash_t(str_uint32) *features, char *feature, uint32_t *feature_id, bool *exists) {
    if (feature == NULL) {
        log_error("feature was NULL\n");
        return false;
    }

    if (features == NULL) {
        log_error("features hashtable was NULL\n");
        return false;
    }

    if (str_uint32_hash_to_id_exists(features, feature, feature_id, exists)) {
        return true;
    }

    return false;
}


inline bool crf_trainer_hash_feature_to_id_exists(crf_trainer_t *self, char *feature, uint32_t *feature_id, bool *exists) {
    return crf_trainer_hash_to_id(self->features, feature, feature_id, exists);
}

inline bool crf_trainer_hash_feature_to_id(crf_trainer_t *self, char *feature, uint32_t *feature_id) {
    bool exists;
    return crf_trainer_hash_feature_to_id_exists(self, feature, feature_id, &exists);
}


inline bool crf_trainer_hash_prev_tag_feature_to_id_exists(crf_trainer_t *self, char *feature, uint32_t *feature_id, bool *exists) {
    return crf_trainer_hash_to_id(self->prev_tag_features, feature, feature_id, exists);
}

inline bool crf_trainer_hash_prev_tag_feature_to_id(crf_trainer_t *self, char *feature, uint32_t *feature_id) {
    bool exists;
    return crf_trainer_hash_feature_to_id_exists(self, feature, feature_id, &exists);
}

inline bool crf_trainer_get_feature_id(crf_trainer_t *self, char *feature, uint32_t *feature_id) {
    return str_uint32_hash_get(self->features, feature, feature_id);
}

inline bool crf_trainer_get_prev_tag_feature_id(crf_trainer_t *self, char *feature, uint32_t *feature_id) {
    return str_uint32_hash_get(self->prev_tag_features, feature, feature_id);
}
