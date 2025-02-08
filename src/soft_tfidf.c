#include "soft_tfidf.h"
#include <math.h>
#include "address_dictionary.h"
#include "float_utils.h"
#include "string_similarity.h"
#include "string_utils.h"
#include "log/log.h"

static soft_tfidf_options_t DEFAULT_SOFT_TFIDF_OPTIONS = {
    .jaro_winkler_min = 0.9,
    .jaro_winkler_min_length = 4,
    .damerau_levenshtein_max = 1,
    .damerau_levenshtein_min_length = 4,
    .possible_affine_gap_abbreviations = true,
    .strict_abbreviation_min_length = 4,
    .strict_abbreviation_sim = 0.99
};


soft_tfidf_options_t soft_tfidf_default_options(void) {
    return DEFAULT_SOFT_TFIDF_OPTIONS;
}

bool compare_canonical(address_expansion_t e1, char **tokens1, phrase_t match1, address_expansion_t e2, char **tokens2, phrase_t match2) {
    bool e1_canonical = e1.canonical_index == NULL_CANONICAL_INDEX;
    bool e2_canonical = e2.canonical_index == NULL_CANONICAL_INDEX;

    if (!e1_canonical && !e2_canonical) {
        return e1.canonical_index == e2.canonical_index;
    } else if (e1_canonical && e2_canonical) {
        if (match1.len != match2.len || match1.len == 0) return false;
        for (size_t i = 0; i < match1.len; i++) {
            char *s1 = tokens1[match1.start + i];
            char *s2 = tokens2[match2.start + i];
            if (!string_equals(s1, s2)) return false;
        }
        return true;
    } else {
        char **canonical_tokens = e1_canonical ? tokens1 : tokens2;
        char *other_canonical = e1_canonical ? address_dictionary_get_canonical(e2.canonical_index) : address_dictionary_get_canonical(e1.canonical_index);
        phrase_t match = e1_canonical ? match1 : match2;

        size_t canonical_index = 0;
        size_t canonical_len = strlen(other_canonical);

        for (size_t i = match.start; i < match.start + match.len; i++) {
            char *canonical_token = canonical_tokens[i];
            size_t canonical_token_len = strlen(canonical_token);

            if (canonical_index + canonical_token_len <= canonical_len && strncmp(other_canonical + canonical_index, canonical_token, canonical_token_len) == 0) {
                canonical_index += canonical_token_len;

                if (i < match.start + match.len - 1 && canonical_index < canonical_len && strncmp(other_canonical + canonical_index, " ", 1) == 0) {
                    canonical_index++;
                }
            } else {
                return false;
            }
        }
        return canonical_index == canonical_len;
    }
}

typedef enum {
    CANONICAL_NO_MATCH = 0,
    NEITHER_CANONICAL,
    SECOND_CANONICAL,
    FIRST_CANONICAL,
    BOTH_CANONICAL
} canonical_match_t;

bool phrases_have_same_canonical(size_t num_tokens1, char **tokens1, size_t num_tokens2, char **tokens2, phrase_t match1, phrase_t match2, canonical_match_t *response) {
    address_expansion_value_t *val1 = address_dictionary_get_expansions(match1.data);
    address_expansion_value_t *val2 = address_dictionary_get_expansions(match2.data);

    if (val1 == NULL || val2 == NULL) return false;

    address_expansion_array *expansions_array1 = val1->expansions;
    address_expansion_array *expansions_array2 = val2->expansions;

    if (expansions_array1 == NULL || expansions_array2 == NULL) return false;

    address_expansion_t *expansions1 = expansions_array1->a;
    address_expansion_t *expansions2 = expansions_array2->a;

    *response = CANONICAL_NO_MATCH;

    bool same_canonical = false;
    for (size_t i = 0; i < expansions_array1->n; i++) {
        address_expansion_t e1 = expansions1[i];

        for (size_t j = 0; j < expansions_array2->n; j++) {
            address_expansion_t e2 = expansions2[j];

            same_canonical = compare_canonical(e1, tokens1, match1, e2, tokens2, match2);
            if (same_canonical) {
                bool e1_canonical = e1.canonical_index == NULL_CANONICAL_INDEX;
                bool e2_canonical = e2.canonical_index == NULL_CANONICAL_INDEX;

                if (e1_canonical && e2_canonical) {
                    *response = BOTH_CANONICAL;
                } else if (e1_canonical) {
                    *response = FIRST_CANONICAL;
                } else if (e2_canonical) {
                    *response = SECOND_CANONICAL;
                } else {
                    *response = NEITHER_CANONICAL;
                }
                break;
            }
        }
        if (same_canonical) break;
    }

    return same_canonical;
}

static inline size_t sum_token_lengths(size_t num_tokens, char **tokens) {
    size_t n = 0;
    for (size_t i = 0; i < num_tokens; i++) {
        char *token = tokens[i];
        n += strlen(token);
    }
    return n;
}


double soft_tfidf_similarity_with_phrases_and_acronyms(size_t num_tokens1, char **tokens1, double *token_scores1, phrase_array *phrases1, uint32_array *ordinal_suffixes1, size_t num_tokens2, char **tokens2, double *token_scores2, phrase_array *phrases2, uint32_array *ordinal_suffixes2, phrase_array *acronym_alignments, phrase_array *multi_word_alignments, soft_tfidf_options_t options, size_t *num_matches) {
    if (token_scores1 == NULL || token_scores2 == NULL) return 0.0;

    if (num_tokens1 > num_tokens2 || (num_tokens1 == num_tokens2 && sum_token_lengths(num_tokens1, tokens1) > sum_token_lengths(num_tokens2, tokens2))) {
        double *tmp_scores = token_scores1;
        token_scores1 = token_scores2;
        token_scores2 = tmp_scores;
        char **tmp_tokens = tokens1;
        tokens1 = tokens2;
        tokens2 = tmp_tokens;

        phrase_array *tmp_phrases = phrases1;
        phrases1 = phrases2;
        phrases2 = tmp_phrases;

        uint32_array *tmp_suffixes = ordinal_suffixes1;
        ordinal_suffixes1 = ordinal_suffixes2;
        ordinal_suffixes2 = tmp_suffixes;

        size_t tmp_num_tokens = num_tokens1;
        num_tokens1 = num_tokens2;
        num_tokens2 = tmp_num_tokens;
    }

    size_t len1 = num_tokens1;
    size_t len2 = num_tokens2;

    double total_sim = 0.0;

    uint32_array **t1_tokens_unicode = NULL;
    uint32_array **t2_tokens_unicode = NULL;

    uint32_array *t1_unicode = NULL;
    uint32_array *t2_unicode = NULL;

    int64_array *phrase_memberships_array1 = NULL;
    int64_array *phrase_memberships_array2 = NULL;
    int64_t *phrase_memberships1 = NULL;
    int64_t *phrase_memberships2 = NULL;

    int64_array *acronym_memberships_array = NULL;
    int64_t *acronym_memberships = NULL;

    int64_array *multi_word_memberships_array = NULL;
    int64_t *multi_word_memberships = NULL;

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

    for (size_t j = 0; j < len2; j++) {
        t2_unicode = unicode_codepoints(tokens2[j]);
        if (t2_unicode == NULL) {
            total_sim = -1.0;
            goto return_soft_tfidf_score;
        }
        t2_tokens_unicode[j] = t2_unicode;
    }


    if (phrases1 != NULL && phrases2 != NULL) {
        phrase_memberships_array1 = int64_array_new();
        phrase_memberships_array2 = int64_array_new();
        token_phrase_memberships(phrases1, phrase_memberships_array1, len1);
        token_phrase_memberships(phrases2, phrase_memberships_array2, len2);

        if (phrase_memberships_array1->n == len1) {
            phrase_memberships1 = phrase_memberships_array1->a;
        }

        if (phrase_memberships_array2->n == len2) {
            phrase_memberships2 = phrase_memberships_array2->a;
        }
    }

    if (acronym_alignments != NULL) {
        acronym_memberships_array = int64_array_new();
        token_phrase_memberships(acronym_alignments, acronym_memberships_array, len2);
        if (acronym_memberships_array->n == len2) {
            acronym_memberships = acronym_memberships_array->a;
        }
    }

    if (multi_word_alignments != NULL) {
        multi_word_memberships_array = int64_array_new();
        token_phrase_memberships(multi_word_alignments, multi_word_memberships_array, len2);
        if (multi_word_memberships_array->n == len2) {
            multi_word_memberships = multi_word_memberships_array->a;
        }
    }

    uint32_t *suffixes1 = NULL;
    uint32_t *suffixes2 = NULL;

    if (ordinal_suffixes1 != NULL && ordinal_suffixes2 != NULL) {
        suffixes1 = ordinal_suffixes1->a;
        suffixes2 = ordinal_suffixes2->a;
    }

    double jaro_winkler_min = options.jaro_winkler_min;
    size_t jaro_winkler_min_length = options.jaro_winkler_min_length;
    size_t damerau_levenshtein_max = options.damerau_levenshtein_max;
    size_t damerau_levenshtein_min_length = options.damerau_levenshtein_min_length;
    size_t strict_abbreviation_min_length = options.strict_abbreviation_min_length;

    bool possible_affine_gap_abbreviations = options.possible_affine_gap_abbreviations;

    double norm1_offset = 0.0;
    double norm2_offset = 0.0;

    size_t matched_tokens = 0;

    for (size_t i = 0; i < len1; i++) {
        uint32_array *t1u = t1_tokens_unicode[i];
        uint32_array *t2u;
        double t1_score = token_scores1[i];

        size_t t1_len = t1u->n;

        log_debug("t1 = %s\n", tokens1[i]);

        double max_sim = 0.0;
        size_t min_dist = t1_len;
        size_t argmax_sim = 0;
        size_t argmin_dist = 0;
        double argmin_dist_sim = 0.0;
        size_t last_abbreviation = 0;
        double last_abbreviation_sim = 0.0;
        bool have_abbreviation = false;
        bool have_strict_abbreviation = false;
        bool have_acronym_match = false;
        size_t last_ordinal_suffix = 0;
        bool have_ordinal_suffix = false;
        phrase_t acronym_phrase = NULL_PHRASE;
        bool have_phrase_match = false;
        int64_t pm1 = phrase_memberships1 != NULL ? phrase_memberships1[i] : NULL_PHRASE_MEMBERSHIP;
        phrase_t p1 = pm1 >= 0 ? phrases1->a[pm1] : NULL_PHRASE;
        phrase_t argmax_phrase = NULL_PHRASE;

        uint32_t ordinal_suffix_i = 0;
        if (suffixes1 != NULL) {
            ordinal_suffix_i = suffixes1[i];
        }

        bool have_multi_word_match = false;
        phrase_t multi_word_phrase = NULL_PHRASE;        

        bool use_jaro_winkler = t1_len >= jaro_winkler_min_length;
        bool use_strict_abbreviation_sim = t1_len >= strict_abbreviation_min_length;
        bool use_damerau_levenshtein = damerau_levenshtein_max > 0 && t1_len >= damerau_levenshtein_min_length;
        log_debug("use_jaro_winkler = %d, use_damerau_levenshtein=%d\n", use_jaro_winkler, use_damerau_levenshtein);

        bool have_equal = false;

        canonical_match_t best_canonical_phrase_response = CANONICAL_NO_MATCH;

        double t2_score;

        log_debug("p1.len = %zu, i = %zu, p1.start = %zu\n", p1.len, i, p1.start);
        if (p1.len > 0 && i > p1.start) {
            log_debug("skipping token\n");
            continue;
        }

        size_t slen1 = strlen(tokens1[i]);

        for (size_t j = 0; j < len2; j++) {
            t2u = t2_tokens_unicode[j];

            log_debug("t2 = %s\n", tokens2[j]);
            int64_t pm2 = phrase_memberships2 != NULL ? phrase_memberships2[j] : NULL_PHRASE_MEMBERSHIP;
            phrase_t p2 = pm2 >= 0 ? phrases2->a[pm2] : NULL_PHRASE;

            uint32_t ordinal_suffix_j = 0;
            if (suffixes2 != NULL) {
                ordinal_suffix_j = suffixes2[j];
            }

            canonical_match_t canonical_response = CANONICAL_NO_MATCH;
            if (p1.len > 0 && p2.len > 0 && phrases_have_same_canonical(num_tokens1, tokens1, num_tokens2, tokens2, p1, p2, &canonical_response)) {
                if (canonical_response > best_canonical_phrase_response) {
                    log_debug("canonical_response = %d\n", canonical_response);
                    best_canonical_phrase_response = canonical_response;
                    argmax_sim = j;
                    argmax_phrase = p2;
                    max_sim = 1.0;
                    have_phrase_match = true;
                    continue;
                }
            }

            if (unicode_equals(t1u, t2u)) {
                max_sim = 1.0;
                argmax_sim = j;
                have_equal = true;
                break;
            }

            if (acronym_memberships != NULL) {
                int64_t acronym_membership = acronym_memberships[j];
                log_debug("acronym_membership = %zd\n", acronym_membership);
                if (acronym_membership >= 0) {
                   acronym_phrase = acronym_alignments->a[acronym_membership];
                   uint32_t acronym_match_index = acronym_phrase.data;
                   if (acronym_match_index == i) {
                        max_sim = 1.0;
                        argmax_sim = j;
                        have_acronym_match = true;
                        log_debug("have acronym match\n");
                        break;
                   }
                }
            }

            if (multi_word_memberships != NULL) {
                int64_t multi_word_membership = multi_word_memberships[j];
                log_debug("multi_word_membership = %zd\n", multi_word_membership);
                if (multi_word_membership >= 0) {
                   multi_word_phrase = multi_word_alignments->a[multi_word_membership];
                   uint32_t multi_word_match_index = multi_word_phrase.data;
                   if (multi_word_match_index == i) {
                        max_sim = 1.0;
                        argmax_sim = j;
                        have_multi_word_match = true;
                        log_debug("have multi-word match\n");
                        break;
                   }
                }
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

            if (possible_affine_gap_abbreviations) {
                bool is_abbreviation = possible_abbreviation_unicode(t1u, t2u);
                if (is_abbreviation) {
                    last_abbreviation = j;
                    last_abbreviation_sim = jaro_winkler;
                    have_abbreviation = true;
                    if (use_strict_abbreviation_sim && possible_abbreviation_unicode_strict(t1u, t2u)) {
                        last_abbreviation_sim = last_abbreviation_sim > options.strict_abbreviation_sim ? last_abbreviation_sim : options.strict_abbreviation_sim;
                    }
                }
            }

            if (ordinal_suffix_i > 0) {
                size_t slen2 = strlen(tokens2[j]);
                if (utf8_common_prefix_len(tokens1[i], tokens2[j], slen2) == slen2) {
                    last_ordinal_suffix = j;
                    have_ordinal_suffix = true;
                }
            } else if (ordinal_suffix_j > 0) {
                if (utf8_common_prefix_len(tokens1[i], tokens2[j], slen1) == slen1) {
                    last_ordinal_suffix = j;
                    have_ordinal_suffix = true;
                }
            }
        }

        // Note: here edit distance, affine gap and abbreviations are only used in the thresholding process.
        // Jaro-Winkler is still used to calculate similarity

        if (!have_acronym_match && !have_phrase_match && !have_multi_word_match) {
            if (have_equal || (use_jaro_winkler && (max_sim > jaro_winkler_min || double_equals(max_sim, jaro_winkler_min)))) {
                log_debug("jaro-winkler, max_sim = %f\n", max_sim);
                t2_score = token_scores2[argmax_sim];
                double jaro_winkler_sim = 0.0;
                if (have_abbreviation && argmax_sim == last_abbreviation) {
                    double abbrev_sim = last_abbreviation_sim > max_sim ? last_abbreviation_sim : max_sim;
                    log_debug("have abbreviation, max(max_sim, last_abbreviation_sim) = %f\n", abbrev_sim);
                    double max_score = 0.0;
                    if (t1_score > t2_score || double_equals(t1_score, t2_score)) {
                        norm2_offset += (t1_score * t1_score) - (t2_score * t2_score);
                        log_debug("t1_score >= t2_score, norm2_offset = %f\n", norm2_offset);
                        max_score = t1_score;
                    } else {
                        norm1_offset += (t2_score * t2_score) - (t1_score * t1_score);
                        log_debug("t2_score > t1_score, norm1_offset = %f\n", norm1_offset);
                        max_score = t2_score;
                    }

                    jaro_winkler_sim = abbrev_sim * max_score * max_score;
                } else {
                    jaro_winkler_sim = max_sim * t1_score * t2_score;
                    log_debug("t1_score = %f, t2_score = %f, jaro_winkler_sim = %f\n", t1_score, t2_score, jaro_winkler_sim);
                }
                total_sim += jaro_winkler_sim;
                matched_tokens++;
            } else if (use_damerau_levenshtein && min_dist <= damerau_levenshtein_max) {
                log_debug("levenshtein, argmin_dist_sim = %f\n", argmin_dist_sim);
                t2_score = token_scores2[argmin_dist];
                if (have_abbreviation && argmin_dist == last_abbreviation) {
                    argmin_dist_sim = last_abbreviation_sim > argmin_dist_sim ? last_abbreviation_sim : argmin_dist_sim;
                }
                total_sim += argmin_dist_sim * t1_score * t2_score;
                matched_tokens++;
            } else if (possible_affine_gap_abbreviations && have_abbreviation) {
                log_debug("have abbreviation, last_abbreviation_sim = %f\n", last_abbreviation_sim);
                t2_score = token_scores2[last_abbreviation];
                total_sim += last_abbreviation_sim * t1_score * t2_score;
                matched_tokens++;
            } else if (have_ordinal_suffix) {
                log_debug("have ordinal suffix from %zu\n", last_ordinal_suffix);
                t2_score = token_scores2[last_ordinal_suffix];
                total_sim += 1.0 * t1_score * t2_score;
                matched_tokens++;
            }
        } else if (have_phrase_match) {
            double p2_score = 0.0;
            for (size_t p = argmax_phrase.start; p < argmax_phrase.start + argmax_phrase.len; p++) {
                t2_score = token_scores2[p];
                p2_score += t2_score * t2_score;
            }

            double p1_score = 0.0;

            for (size_t p = p1.start; p < p1.start + p1.len; p++) {
                double t1_score_p = token_scores1[p];
                p1_score += t1_score_p * t1_score_p;
            }

            total_sim += sqrt(p1_score) * sqrt(p2_score);

            matched_tokens += p1.len;
            log_debug("have_phrase_match\n");
        } else if (have_multi_word_match) {
            double multi_word_score = 0.0;
            for (size_t p = multi_word_phrase.start; p < multi_word_phrase.start + multi_word_phrase.len; p++) {
                t2_score = token_scores2[p];
                multi_word_score += t2_score * t2_score;
            }

            double norm_multi_word_score = sqrt(multi_word_score);

            double max_multi_word_score = 0.0;
            if (t1_score > norm_multi_word_score || double_equals(t1_score, norm_multi_word_score)) {
                norm2_offset += (t1_score * t1_score) - multi_word_score;
                log_debug("t1_score >= norm_multi_word_score, norm2_offset = %f\n", norm2_offset);
                max_multi_word_score = t1_score;
            } else {
                norm1_offset += multi_word_score - (t1_score * t1_score);
                log_debug("norm_multi_word_score > t1_score, norm1_offset = %f\n", norm1_offset);
                max_multi_word_score = norm_multi_word_score;
            }

            log_debug("max_multi_word_score = %f\n", max_multi_word_score);

            total_sim += max_multi_word_score * max_multi_word_score;

            log_debug("have multi-word match\n");
            matched_tokens++;
        } else if (have_acronym_match) {
            double acronym_score = 0.0;
            for (size_t p = acronym_phrase.start; p < acronym_phrase.start + acronym_phrase.len; p++) {
                t2_score = token_scores2[p];
                acronym_score += t2_score * t2_score;
            }

            double norm_acronym_score = sqrt(acronym_score);

            double max_acronym_score = 0.0;
            if (t1_score > norm_acronym_score || double_equals(t1_score, norm_acronym_score)) {
                norm2_offset += (t1_score * t1_score) - acronym_score;
                log_debug("t1_score >= norm_acronym_score, norm2_offset = %f\n", norm2_offset);
                max_acronym_score = t1_score;
            } else {
                norm1_offset += acronym_score - (t1_score * t1_score);
                log_debug("norm_acronym_score > t1_score, norm1_offset = %f\n", norm1_offset);
                max_acronym_score = norm_acronym_score;
            }

            log_debug("max_acronym_score = %f\n", max_acronym_score);

            total_sim += max_acronym_score * max_acronym_score;

            log_debug("have_acronym_match\n");
            matched_tokens++;
        }

        log_debug("total sim = %f\n", total_sim);
    }

    log_debug("matched_tokens = %zu\n", matched_tokens);

    *num_matches = matched_tokens;

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

    if (phrase_memberships_array1 != NULL) {
        int64_array_destroy(phrase_memberships_array1);
    }

    if (phrase_memberships_array2 != NULL) {
        int64_array_destroy(phrase_memberships_array2);
    }

    if (acronym_memberships_array != NULL) {
        int64_array_destroy(acronym_memberships_array);
    }

    if (multi_word_memberships_array != NULL) {
        int64_array_destroy(multi_word_memberships_array);
    }

    double norm = sqrt(double_array_sum_sq(token_scores1, num_tokens1) + norm1_offset) * sqrt(double_array_sum_sq(token_scores2, num_tokens2) + norm2_offset);
    log_debug("total_sim = %f, norm1_offset = %f, norm2_offset = %f, norm = %f\n", total_sim, norm1_offset, norm2_offset, norm);

    double total_sim_norm = total_sim / norm;
    return total_sim_norm > 1.0 ? 1.0 : total_sim_norm;
}


double soft_tfidf_similarity(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, soft_tfidf_options_t options, size_t *num_matches) {
    return soft_tfidf_similarity_with_phrases_and_acronyms(num_tokens1, tokens1, token_scores1, NULL, NULL, num_tokens2, tokens2, token_scores2, NULL, NULL, NULL, NULL, options, num_matches);
}
