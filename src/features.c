#include "features.h"


void feature_array_add(cstring_array *features, size_t count, ...) {
    if (count <= 0) {
        return;        
    }

    va_list args;
    va_start(args, count);

    cstring_array_start_token(features);

    for (size_t i = 0; i < count - 1; i++) {
        char *arg = va_arg(args, char *);
        char_array_append(features->str, arg);
        char_array_append(features->str, FEATURE_SEPARATOR_CHAR);
    }

    char *arg = va_arg(args, char *);
    char_array_append(features->str, arg);
    char_array_terminate(features->str);

    va_end(args);
}


bool feature_counts_update(khash_t(str_uint32) *features, char *feature, int count) {
    khiter_t k;

    k = kh_get(str_uint32, features, feature);
    if (k == kh_end(features)) {
        int ret;
        k = kh_put(str_uint32, features, feature, &ret);
        if (ret < 0) return false;

        kh_value(features, k) = count;
    } else {
        kh_value(features, k) += count;
    }
    return true;
}
