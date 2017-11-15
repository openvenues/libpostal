#include "features.h"


void feature_array_add(cstring_array *features, size_t count, ...) {
    if (count <= 0) {
        return;        
    }

    va_list args;
    va_start(args, count);

    cstring_array_start_token(features);

    bool strip_separator = true;
    char_array_add_vjoined(features->str, FEATURE_SEPARATOR_CHAR, strip_separator, count, args);
    va_end(args);
}


void feature_array_add_printf(cstring_array *features, char *format, ...) {
    va_list args;
    va_start(args, format);
    cstring_array_start_token(features);
    char_array_cat_vprintf(features->str, format, args);
    va_end(args);
}


bool feature_counts_update_or_add(khash_t(str_double) *features, char *feature, double count, bool copy, bool add) {
    khiter_t k;

    k = kh_get(str_double, features, feature);
    if (k == kh_end(features)) {
        int ret;
        char *str;
        if (copy) {
            str = strdup(feature);
        } else {
            str = feature;
        }

        k = kh_put(str_double, features, str, &ret);
        if (ret < 0) return false;

        kh_value(features, k) = count;
    } else if (add) {
        kh_value(features, k) += count;
    } else {
        kh_value(features, k) = count;
    }
    return true;
}


inline bool feature_counts_add(khash_t(str_double) *features, char *feature, double count) {
    return feature_counts_update_or_add(features, feature, count, true, true);
}

inline bool feature_counts_add_no_copy(khash_t(str_double) *features, char *feature, double count) {
    return feature_counts_update_or_add(features, feature, count, false, true);
}

inline bool feature_counts_update(khash_t(str_double) *features, char *feature, double count) {
    return feature_counts_update_or_add(features, feature, count, true, false);
}

inline bool feature_counts_update_no_copy(khash_t(str_double) *features, char *feature, double count) {
    return feature_counts_update_or_add(features, feature, count, false, false);
}
