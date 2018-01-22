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
    .possible_affine_gap_abbreviations = true
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


double soft_tfidf_similarity_with_phrases_and_acronyms(size_t num_tokens1, char **tokens1, double *token_scores1, phrase_array *phrases1, size_t num_tokens2, char **tokens2, double *token_scores2, phrase_array *phrases2, phrase_array *acronym_alignments, soft_tfidf_options_t options, size_t *num_matches) {
    if (token_scores1 == NULL || token_scores2 == NULL) return 0.0;

    if (num_tokens1 > num_tokens2) {
        double *tmp_scores = token_scores1;
        token_scores1 = token_scores2;
        token_scores2 = tmp_scores;
        char **tmp_tokens = tokens1;
        tokens1 = tokens2;
        tokens2 = tmp_tokens;

        phrase_array *tmp_phrases = phrases1;
        phrases1 = phrases2;
        phrases2 = tmp_phrases;

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

    int64_array *phrase_memberships_array1 = NULL;
    int64_array *phrase_memberships_array2 = NULL;
    int64_t *phrase_memberships1 = NULL;
    int64_t *phrase_memberships2 = NULL;

    int64_array *acronym_memberships_array = NULL;
    int64_t *acronym_memberships = NULL;

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

    double jaro_winkler_min = options.jaro_winkler_min;
    size_t jaro_winkler_min_length = options.jaro_winkler_min_length;
    size_t damerau_levenshtein_max = options.damerau_levenshtein_max;
    size_t damerau_levenshtein_min_length = options.damerau_levenshtein_min_length;

    bool possible_affine_gap_abbreviations = options.possible_affine_gap_abbreviations;

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
        bool have_acronym_match = false;
        phrase_t acronym_phrase = NULL_PHRASE;
        bool have_phrase_match = false;
        int64_t pm1 = phrase_memberships1 != NULL ? phrase_memberships1[i] : NULL_PHRASE_MEMBERSHIP;
        phrase_t p1 = pm1 >= 0 ? phrases1->a[pm1] : NULL_PHRASE;
        phrase_t argmax_phrase = NULL_PHRASE;
    
        bool use_jaro_winkler = t1_len >= jaro_winkler_min_length;
        bool use_damerau_levenshtein = damerau_levenshtein_max > 0 && t1_len >= damerau_levenshtein_min_length;
        log_debug("use_jaro_winkler = %d, use_damerau_levenshtein=%d\n", use_jaro_winkler, use_damerau_levenshtein);

        canonical_match_t best_canonical_phrase_response = CANONICAL_NO_MATCH;

        double t2_score;

        log_debug("p1.len = %zu, i = %zu, p1.start = %zu\n", p1.len, i, p1.start);
        if (p1.len > 0 && i > p1.start) {
            log_debug("skipping token\n");
            continue;
        }

        for (size_t j = 0; j < len2; j++) {
            t2u = t2_tokens_unicode[j];

            log_debug("t2 = %s\n", tokens2[j]);
            int64_t pm2 = phrase_memberships2 != NULL ? phrase_memberships2[j] : NULL_PHRASE_MEMBERSHIP;
            phrase_t p2 = pm2 >= 0 ? phrases2->a[pm2] : NULL_PHRASE;

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
                }
            }

        }

        // Note: here edit distance, affine gap and abbreviations are only used in the thresholding process.
        // Jaro-Winkler is still used to calculate similarity

        if (!have_acronym_match && !have_phrase_match) {
            if (use_jaro_winkler && (max_sim > jaro_winkler_min || double_equals(max_sim, jaro_winkler_min))) {
                log_debug("jaro-winkler, max_sim = %f\n", max_sim);
                t2_score = token_scores2[argmax_sim];
                total_sim += max_sim * t1_score * t2_score;
                matched_tokens++;
            } else if (use_damerau_levenshtein && min_dist <= damerau_levenshtein_max) {
                log_debug("levenshtein, argmin_dist_sim = %f\n", argmin_dist_sim);
                t2_score = token_scores2[argmin_dist];
                total_sim += argmin_dist_sim * t1_score * t2_score;
                matched_tokens++;
            } else if (possible_affine_gap_abbreviations && have_abbreviation) {
                log_debug("have abbreviation, last_abbreviation_sim = %f\n", last_abbreviation_sim);
                t2_score = token_scores2[last_abbreviation];
                total_sim += last_abbreviation_sim * t1_score * t2_score;
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
        } else {
            for (size_t p = acronym_phrase.start; p < acronym_phrase.start + acronym_phrase.len; p++) {
                t2_score = token_scores2[p];
                total_sim += t1_score * t2_score;
            }

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

    double norm = double_array_l2_norm(token_scores1, num_tokens1) * double_array_l2_norm(token_scores2, num_tokens2);
    log_debug("total_sim = %f, norm = %f\n", total_sim, norm);

    return total_sim / norm;
}


double soft_tfidf_similarity(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, soft_tfidf_options_t options, size_t *num_matches) {
    return soft_tfidf_similarity_with_phrases_and_acronyms(num_tokens1, tokens1, token_scores1, NULL, num_tokens2, tokens2, token_scores2, NULL, NULL, options, num_matches);
}