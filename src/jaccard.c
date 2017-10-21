#include "jaccard.h"


double jaccard_similarity(khash_t(str_set) *s1, khash_t(str_set) *s2) {
    if (s1 == NULL || s2 == NULL) return -1.0;

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