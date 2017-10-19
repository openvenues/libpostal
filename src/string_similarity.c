#include "string_similarity.h"
#include "string_utils.h"


size_t damerau_levenshtein_distance_unicode(uint32_array *u1_array, uint32_array *u2_array, size_t replace_cost) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    size_t num_bytes = (len1 + 1) * sizeof(size_t);

    size_t *column = malloc(num_bytes);
    for (size_t y = 1; y <= len1; y++) {
        column[y] = y;
    }

    size_t transpose_diag = 0;
    size_t last_diag = 0;

    for (size_t x = 1; x <= len2; x++) {
        column[0] = x;
        for (size_t y = 1, last_diag = x - 1; y <= len1; y++) {
            size_t old_diag = column[y];
            size_t cost = (u1[y - 1] == u2[x - 1] ? 0 : 1);

            size_t v1 = column[y] + 1;
            size_t v2 = column[y - 1] + 1;
            size_t v3 = last_diag + cost;

            size_t min = v1;
            if (v2 < min) min = v2;
            if (v3 < min) min = v3;

            if (x > 1 && y > 1 && u1[y - 1] == u2[x - 2] && u1[y - 2] == u2[x - 1]) {
                size_t v4 = transpose_diag + cost;
                if (v4 < min) min = v4;
            }

            column[y] = min;

            last_diag = old_diag;
        }
        transpose_diag = last_diag;
    }

    size_t dist = column[len1];
    free(column);
    return dist;
}

ssize_t damerau_levenshtein_distance_replace_cost(char *s1, char *s2, size_t replace_cost) {
    if (s1 == NULL || s2 == NULL) return -1;

    uint32_array *u1 = unicode_codepoints(s1);
    if (u1 == NULL) return -1.0;

    uint32_array *u2 = unicode_codepoints(s2);

    if (u2 == NULL) {
        uint32_array_destroy(u1);
        return -1.0;
    }

    ssize_t lev = damerau_levenshtein_distance_unicode(u1, u2, replace_cost);

    uint32_array_destroy(u1);
    uint32_array_destroy(u2);
    return lev;
}

ssize_t damerau_levenshtein_distance(char *s1, char *s2) {
    return damerau_levenshtein_distance_replace_cost(s1, s2, 0);
}

double jaro_distance_unicode(uint32_array *u1_array, uint32_array *u2_array) {
    if (u1_array == NULL || u2_array == NULL) return -1.0;

    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;
    // If both strings are zero-length, return 1. If only one is, return 0
    if (len1 == 0) return len2 == 0 ? 1.0 : 0.0;

    size_t max_len = len1 > len2 ? len1 : len2;
    size_t match_distance = (max_len / 2) - 1;

    uint8_t *u1_matches = calloc(len2, sizeof(uint8_t));
    uint8_t *u2_matches = calloc(len1, sizeof(uint8_t));

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    double matches = 0.0;
    double transpositions = 0.0;

    size_t i = 0;
 
    // count matches
    for (size_t i = 0; i < len1; i++) {
        // start and end take into account the match distance
        size_t start = i > match_distance ? i - match_distance : 0;
        size_t end = (i + match_distance + 1) < len2 ? i + match_distance + 1 : len2;

        for (size_t k = start; k < end; k++) {
            // already a match at k
            if (u2_matches[k]) continue;
            // codepoints not equal
            if (u1[i] != u2[k]) continue;
            // otherwise record a match on both sides and increment counter
            u1_matches[i] = true;
            u2_matches[k] = true;
            matches++;
            break;
        }
    }

    if (matches == 0) {
        free(u1_matches);
        free(u2_matches);
        return 0.0;
    }


    // count transpositions
    size_t k = 0;
    for (size_t i = 0; i < len1; i++) {
        // wait for a match in u1
        if (!u1_matches[i]) continue;
        // get the next matched character in u2
        while (!u2_matches[k]) k++;
        // it's a transposition
        if (u1[i] != u2[k]) transpositions++;
        k++;
    }

    // transpositions double-count transposed characters, so divide by 2
    transpositions /= 2.0;

    free(u1_matches);
    free(u2_matches);

    // Jaro distance
    return ((matches / len1) +
            (matches / len2) +
            ((matches - transpositions) / matches)) / 3.0;
}

double jaro_distance(const char *s1, const char *s2) {
    if (s1 == NULL || s2 == NULL) {
        return -1.0;
    }

    uint32_array *u1 = unicode_codepoints(s1);
    if (u1 == NULL) return -1.0;

    uint32_array *u2 = unicode_codepoints(s2);

    if (u2 == NULL) {
        uint32_array_destroy(u1);
        return -1.0;
    }

    double jaro = jaro_distance_unicode(u1, u2);
    uint32_array_destroy(u1);
    uint32_array_destroy(u2);
    return jaro;
}

double jaro_winkler_distance_prefix_threshold(const char *s1, const char *s2, double prefix_scale, double bonus_threshold) {
    if (s1 == NULL || s2 == NULL) {
        return -1.0;
    }

    uint32_array *u1_array = unicode_codepoints(s1);
    if (u1_array == NULL) return -1.0;

    uint32_array *u2_array = unicode_codepoints(s2);

    if (u2_array == NULL) {
        uint32_array_destroy(u1_array);
        return -1.0;
    }

    double jaro = jaro_distance_unicode(u1_array, u2_array);

    double j;

    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    size_t m = len1 < len2 ? len1 : len2;

    size_t i = 0;
    for (; i < m; i++) {
        if (u1[i] != u2[i]) break;
    }

    double jaro_winkler = jaro;

    if (jaro >= bonus_threshold) {
        jaro_winkler += (1.0 - jaro_winkler) * i * prefix_scale;
    }

    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);

    return jaro_winkler > 1.0 ? 1.0 : jaro_winkler;
}

inline double jaro_winkler_distance(const char *s1, const char *s2) {
    return jaro_winkler_distance_prefix_threshold(s1, s2, DEFAULT_JARO_WINKLER_PREFIX_SCALE, DEFAULT_JARO_WINKLER_BONUS_THRESHOLD);
}
