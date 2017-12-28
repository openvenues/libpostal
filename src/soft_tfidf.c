#include "soft_tfidf.h"
#include "float_utils.h"
#include "string_similarity.h"
#include "string_utils.h"

static soft_tfidf_options_t DEFAULT_SOFT_TFIDF_OPTIONS = {
    .jaro_winkler_min = 0.9,
    .damerau_levenshtein_max = 1,
    .damerau_levenshtein_min_length = 4,
    .use_abbreviations = true
};


soft_tfidf_options_t soft_tfidf_default_options(void) {
    return DEFAULT_SOFT_TFIDF_OPTIONS;
}


double soft_tfidf_similarity(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, soft_tfidf_options_t options) {
    if (token_scores1 == NULL || token_scores2 == NULL) return 0.0;

    if (num_tokens2 < num_tokens1) {
        double *tmp_scores = token_scores1;
        token_scores1 = token_scores2;
        token_scores2 = tmp_scores;
        char **tmp_tokens = tokens1;
        tokens1 = tokens2;
        tokens2 = tmp_tokens;

        size_t tmp_num_tokens = num_tokens1;
        num_tokens1 = num_tokens2;
        num_tokens2 = tmp_num_tokens;
    }

    size_t len1 = num_tokens1;
    size_t len2 = num_tokens2;

    double total_sim = 0.0;

    uint32_array **t1_tokens_unicode = NULL;
    uint32_array **t2_tokens_unicode = NULL;

    uint32_array *t1_unicode;
    uint32_array *t2_unicode;

    t1_tokens_unicode = calloc(len1, sizeof(uint32_array *));
    if (t1_tokens_unicode == NULL) {
        total_sim = -1.0;
        goto return_soft_tfidf_score;
    }
    for (size_t i = 0; i < len1; i++) {
        t1_unicode = unicode_codepoints(tokens1[i]);
        if (t1_unicode == NULL) {
            total_sim = -1.0;
            goto return_soft_tfidf_score;
        }
        t1_tokens_unicode[i] = t1_unicode;
    }

    t2_tokens_unicode = calloc(len2, sizeof(uint32_array *));
    if (t2_tokens_unicode == NULL) {
        total_sim = -1.0;
        goto return_soft_tfidf_score;
    }

    for (size_t i = 0; i < len2; i++) {
        t2_unicode = unicode_codepoints(tokens2[i]);
        if (t2_unicode == NULL) {
            total_sim = -1.0;
            goto return_soft_tfidf_score;
        }
        t2_tokens_unicode[i] = t2_unicode;
    }

    double jaro_winkler_min = options.jaro_winkler_min;
    size_t damerau_levenshtein_max = options.damerau_levenshtein_max;
    size_t damerau_levenshtein_min_length = options.damerau_levenshtein_min_length;
    bool use_damerau_levenshtein = damerau_levenshtein_max > 0 && len1 >= damerau_levenshtein_min_length;

    bool use_abbreviations = options.use_abbreviations;

    for (size_t i = 0; i < len1; i++) {
        uint32_array *t1u = t1_tokens_unicode[i];
        uint32_array *t2u;
        char *t1 = tokens1[i];
        double t1_score = token_scores1[i];

        double max_sim = 0.0;
        size_t min_dist = t1u->n;
        size_t argmax_sim = 0;
        size_t argmin_dist = 0;
        double argmin_dist_sim = 0.0;
        size_t last_abbreviation = 0;
        double last_abbreviation_sim = 0.0;
        bool have_abbreviation = false;        
        double t2_score;

        for (size_t j = 0; j < len2; j++) {
            char *t2 = tokens2[j];
            t2u = t2_tokens_unicode[j];
            if (unicode_equals(t1u, t2u)) {
                max_sim = 1.0;
                argmax_sim = j;
                break;
            }

            double jaro_winkler = jaro_winkler_distance_unicode(t1u, t2u);
            if (jaro_winkler > max_sim) {
                max_sim = jaro_winkler;
                argmax_sim = j;
            }

            if (use_damerau_levenshtein) {
                size_t replace_cost = 0;
                ssize_t dist = damerau_levenshtein_distance_unicode(t1u, t2u, replace_cost);
                if (dist >= 0 && dist < min_dist) {
                    min_dist = (size_t)dist;
                    argmin_dist = j;
                    argmin_dist_sim = jaro_winkler;
                }
            }

            if (use_abbreviations) {
                bool is_abbreviation = possible_abbreviation_unicode(t1u, t2u);
                if (is_abbreviation) {
                    last_abbreviation = j;
                    last_abbreviation_sim = jaro_winkler;
                    have_abbreviation = true;
                }
            }
        }

        // Note: here edit distance, affine gap and abbreviations are only used in the thresholding process.
        // Jaro-Winkler is still used to calculate similarity

        if (max_sim > jaro_winkler_min || double_equals(max_sim, jaro_winkler_min)) {
            t2_score = token_scores2[argmax_sim];
            total_sim += max_sim * t1_score * t2_score;
        } else if (use_damerau_levenshtein && min_dist <= damerau_levenshtein_max) {
            t2_score = token_scores2[argmin_dist];
            total_sim += argmin_dist_sim * t1_score * t2_score;
        } else if (use_abbreviations && have_abbreviation) {
            t2_score = token_scores2[last_abbreviation];
            total_sim += last_abbreviation_sim * t1_score * t2_score;
        }
    }

return_soft_tfidf_score:
    if (t1_tokens_unicode != NULL) {
        for (size_t i = 0; i < len1; i++) {
            t1_unicode = t1_tokens_unicode[i];
            if (t1_unicode != NULL) {
                uint32_array_destroy(t1_unicode);
            }
        }
        free(t1_tokens_unicode);
    }

    if (t2_tokens_unicode != NULL) {
        for (size_t i = 0; i < len2; i++) {
            t2_unicode = t2_tokens_unicode[i];
            if (t2_unicode != NULL) {
                uint32_array_destroy(t2_unicode);
            }
        }
        free(t2_tokens_unicode);
    }

    return total_sim;
}