#include "minibatch.h"
#include "float_utils.h"

#define BIAS_FEATURE_ID 0

bool count_features_minibatch(khash_t(str_double) *feature_counts, feature_count_array *minibatch, bool unique) {
    const char *feature;
    uint32_t feature_id;
    double count;

    size_t i;
    size_t m = minibatch->n;

    for (i = 0; i < minibatch->n; i++) {
        khash_t(str_double) *counts = minibatch->a[i];

        kh_foreach(counts, feature, count, {
            // If unique is true, count features once per example
            double value = unique ? 1.0 : count;
            if (!feature_counts_add(feature_counts, (char *)feature, value)) {
                return false;
            }
        })
    }

    return true;
}

bool count_labels_minibatch(khash_t(str_uint32) *label_ids, cstring_array *labels) {
    uint32_t i;
    char *label;

    cstring_array_foreach(labels, i, label, {
        khiter_t k = kh_get(str_uint32, label_ids, label);

        if (k != kh_end(label_ids)) {
            kh_value(label_ids, k)++;
        } else {
            int ret = 0;
            k = kh_put(str_uint32, label_ids, strdup(label), &ret);
            if (ret < 0) {
                return false;
            }
            kh_value(label_ids, k) = 1;
        }

    })

    return true;
}

trie_t *select_features_threshold(khash_t(str_double) *feature_counts, double threshold) {
    const char *feature;
    double count;

    int ret = 0;
    // First feature is the bias unit, so start from 1
    uint32_t feature_id = 1;

    khash_t(str_uint32) *feature_ids = kh_init(str_uint32);

    size_t n = kh_size(feature_counts);

    bool reversed = true;
    char **sorted_keys = str_double_hash_sort_keys_by_value(feature_counts, reversed);
    log_info("Sort done\n");

    for (size_t i = 0; i < n; i++) {
        char *key = sorted_keys[i];
        khiter_t k = kh_get(str_double, feature_counts, key);
        if (k == kh_end(feature_counts)) {
            goto exit_destroy_feature_ids;
        }

        if (strlen(key) == 0) continue;

        count = kh_value(feature_counts, k);
        if (count < threshold && !double_equals(count, threshold)) continue;

        // feature_ids is a local hash, don't need to strdup the key on put
        k = kh_put(str_uint32, feature_ids, key, &ret);
        if (ret < 0) {
            goto exit_destroy_feature_ids;
        }

        kh_value(feature_ids, k) = feature_id++;
    }

    trie_t *trie = trie_new_from_hash(feature_ids);

    free(sorted_keys);
    kh_destroy(str_uint32, feature_ids);
    return trie;

exit_destroy_feature_ids:
    free(sorted_keys);
    kh_destroy(str_uint32, feature_ids);
    return NULL;
}

khash_t(str_uint32) *select_labels_threshold(khash_t(str_uint32) *label_counts, uint32_t threshold) {
    const char *label;
    uint32_t count;

    int ret = 0;
    uint32_t label_id = 0;

    khash_t(str_uint32) *label_ids = kh_init(str_uint32);

    size_t n = kh_size(label_counts);

    bool reversed = true;
    char **sorted_keys = str_uint32_hash_sort_keys_by_value(label_counts, reversed);

    for (size_t i = 0; i < n; i++) {
        char *label = sorted_keys[i];
        khiter_t k = kh_get(str_uint32, label_counts, label);
        if (k == kh_end(label_counts)) {
            goto exit_destroy_label_ids;
        }

        count = kh_value(label_counts, k);
        if (count < threshold) continue;

        k = kh_put(str_uint32, label_ids, label, &ret);
        if (ret < 0) {
            goto exit_destroy_label_ids;
        }

        kh_value(label_ids, k) = label_id++;
    }

    free(sorted_keys);
    return label_ids;
exit_destroy_label_ids:
    free(sorted_keys);
    kh_destroy(str_uint32, label_ids);
    return NULL;
}

sparse_matrix_t *feature_matrix(trie_t *feature_ids, feature_count_array *feature_counts) {
    if (feature_ids == NULL || feature_counts == NULL) return NULL;

    const char *feature;
    uint32_t feature_id;
    double count;

    size_t i;
    size_t m = feature_counts->n;
    // Add one feature for bias unit
    size_t n = trie_num_keys(feature_ids) + 1;

    sparse_matrix_t *matrix = sparse_matrix_new_shape(m, n);

    for (i = 0; i < m; i++) {
        khash_t(str_double) *counts = feature_counts->a[i];
        sparse_matrix_append(matrix, BIAS_FEATURE_ID, 1.0);

        kh_foreach(counts, feature, count, {
            if (!trie_get_data(feature_ids, (char *)feature, &feature_id)) {
                continue;
            }
            sparse_matrix_append(matrix, feature_id, count);
        })

        sparse_matrix_finalize_row(matrix);
    }

    return matrix;
}

sparse_matrix_t *feature_vector(trie_t *feature_ids, khash_t(str_double) *feature_counts) {
    const char *feature;
    uint32_t feature_id;
    double count;

    size_t m = 1;
    // Add one feature for bias unit
    size_t n = trie_num_keys(feature_ids) + 1;

    sparse_matrix_t *matrix = sparse_matrix_new_shape(m, n);

    sparse_matrix_append(matrix, BIAS_FEATURE_ID, 1.0);
    kh_foreach(feature_counts, feature, count, {
        if (!trie_get_data(feature_ids, (char *)feature, &feature_id)) {
            continue;
        }
        sparse_matrix_append(matrix, feature_id, count);
    })

    sparse_matrix_finalize_row(matrix);

    return matrix;   
}

uint32_array *label_vector(khash_t(str_uint32) *label_ids, cstring_array *labels) {
    uint32_t i;
    char *label;
    uint32_t label_id;

    uint32_array *array = uint32_array_new_size(cstring_array_num_strings(labels));

    cstring_array_foreach(labels, i, label, {
        khiter_t k = kh_get(str_uint32, label_ids, label);

        if (k != kh_end(label_ids)) {
            label_id = kh_value(label_ids, k);
            uint32_array_push(array, label_id);
        }

    });

    return array;
}

