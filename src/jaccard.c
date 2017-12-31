#include "jaccard.h"

double jaccard_similarity(khash_t(str_set) *s1, khash_t(str_set) *s2) {
    if (s1 == NULL || s2 == NULL) return 0.0;

    size_t set_intersection = 0;
    size_t set_union = 0;

    khiter_t k;
    const char *key;

    kh_foreach_key(s1, key, {
        k = kh_get(str_set, s2, key);
        if (k != kh_end(s2)) {
            set_intersection++;
        } else {
            set_union++;
        }
    });

    // set_union contains all the keys that were in s1 but not s2
    // so just add all the keys in s2 to complete the union
    set_union += kh_size(s2);

    return (double)set_intersection / set_union;
}


double jaccard_similarity_string_arrays(size_t num_strings1, char **strings1, size_t num_strings2, char **strings2) {
    if (strings1 == NULL || strings2 == NULL || num_strings1 == 0 || num_strings2 == 0) return 0.0;

    khash_t(str_set) *string_set1 = kh_init(str_set);
    if (string_set1 == NULL) return 0.0;

    kh_resize(str_set, string_set1, num_strings1);
    int ret = 0;

    khiter_t k;

    for (size_t i = 0; i < num_strings1; i++) {
        char *str1 = strings1[i];
        k = kh_put(str_set, string_set1, str1, &ret);
        if (ret < 0) {
            kh_destroy(str_set, string_set1);
            return 0.0;
        }
    }

    khash_t(str_set) *string_set2 = kh_init(str_set);
    if (string_set2 == NULL) {
        kh_destroy(str_set, string_set1);
        return 0.0;
    }
    kh_resize(str_set, string_set2, num_strings2);
    for (size_t i = 0; i < num_strings2; i++) {
        char *str2 = strings2[i];
        k = kh_put(str_set, string_set2, str2, &ret);
        if (ret < 0) {
            kh_destroy(str_set, string_set1);
            kh_destroy(str_set, string_set2);
            return 0.0;
        }
    }

    double sim = jaccard_similarity(string_set1, string_set2);
    kh_destroy(str_set, string_set1);
    kh_destroy(str_set, string_set2);
    return sim;
}
