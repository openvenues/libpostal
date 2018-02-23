#include "string_similarity.h"
#include "string_utils.h"

#include <limits.h>

static affine_gap_edits_t NULL_AFFINE_GAP_EDITS = {
    .num_matches = 0,
    .num_mismatches = 0,
    .num_transpositions = 0,
    .num_gap_opens = 0,
    .num_gap_extensions = 0
};

typedef enum {
    AFFINE_CHAR_MATCH,
    AFFINE_CHAR_MISMATCH,
    AFFINE_TRANSPOSITION,
    AFFINE_GAP_OPEN,
    AFFINE_GAP_EXTEND
} affine_gap_op;

static inline bool space_or_equivalent(int32_t c) {
    int cat = utf8proc_category(c);
    return utf8_is_whitespace(c) || utf8_is_hyphen(c) || utf8_is_punctuation(cat);
}

affine_gap_edits_t affine_gap_distance_unicode_costs(uint32_array *u1_array, uint32_array *u2_array, size_t start_gap_cost, size_t extend_gap_cost, size_t match_cost, size_t mismatch_cost, size_t transpose_cost) {
    if (u1_array->n < u2_array->n) {
        uint32_array *tmp_array = u1_array;
        u1_array = u2_array;
        u2_array = tmp_array;
    }

    size_t m = u1_array->n;
    size_t n = u2_array->n;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    affine_gap_edits_t edits = NULL_AFFINE_GAP_EDITS;

    if (unicode_equals(u1_array, u2_array)) {
        edits.num_matches = n;
        return edits;
    }

    size_t num_bytes = (m + 1) * sizeof(size_t);

    size_t *C = malloc(num_bytes);
    if (C == NULL) {
        return NULL_AFFINE_GAP_EDITS;
    }

    size_t *D = malloc(num_bytes);
    if (D == NULL) {
        free(C);
        return NULL_AFFINE_GAP_EDITS;
    }

    affine_gap_edits_t *E = malloc((m + 1) * sizeof(affine_gap_edits_t));
    if (E == NULL) {
        free(C);
        free(D);
        return NULL_AFFINE_GAP_EDITS;
    }

    affine_gap_edits_t *ED = malloc((m + 1) * sizeof(affine_gap_edits_t));
    if (ED == NULL) {
        free(C);
        free(D);
        free(E);
        return NULL_AFFINE_GAP_EDITS;
    }

    size_t e = 0, c = 0, s = 0;

    C[0] = 0;
    E[0] = NULL_AFFINE_GAP_EDITS;
    size_t t = start_gap_cost;

    affine_gap_edits_t base_edits = NULL_AFFINE_GAP_EDITS;
    base_edits.num_gap_opens++;

    for (size_t j = 1; j < m + 1; j++) {
        t += extend_gap_cost;
        C[j] = t;
        D[j] = t + start_gap_cost;
        base_edits.num_gap_extensions++;
        E[j] = base_edits;
        ED[j] = base_edits;
    }

    t = start_gap_cost;
    base_edits = NULL_AFFINE_GAP_EDITS;
    base_edits.num_gap_opens++;

    affine_gap_edits_t current_edits = NULL_AFFINE_GAP_EDITS;
    affine_gap_edits_t prev_char_edits = NULL_AFFINE_GAP_EDITS;
    affine_gap_edits_t prev_row_prev_char_edits = NULL_AFFINE_GAP_EDITS;

    bool in_gap = false;

    for (size_t i = 1; i < n + 1; i++) {
        // s = CC[0]
        s = C[0];
        uint32_t c2 = u2[i - 1];
        // CC[0] = c = t = t + h
        t += extend_gap_cost;
        c = t;
        C[0] = c;
        
        prev_row_prev_char_edits = E[0];
        base_edits.num_gap_extensions++;
        prev_char_edits = base_edits;
        E[0] = prev_char_edits;

        // e = t + g
        e = t + start_gap_cost;

        affine_gap_op op = AFFINE_GAP_OPEN;

        ssize_t match_at = -1;

        size_t min_at = 0;
        size_t min_cost = SIZE_MAX;

        for (size_t j = 1; j < m + 1; j++) {
            // insertion
            // e = min(e, c + g) + h
            size_t min = e;
            uint32_t c1 = u1[j - 1];

            affine_gap_op insert_op = AFFINE_GAP_OPEN;

            if ((c + start_gap_cost) < min) {
                min = c + start_gap_cost;
                insert_op = AFFINE_GAP_OPEN;
            } else {
                insert_op = AFFINE_GAP_EXTEND;
            }

            e = min + extend_gap_cost;

            // deletion
            // DD[j] = min(DD[j], CC[j] + g) + h

            affine_gap_op delete_op = AFFINE_GAP_OPEN;

            min = D[j];
            affine_gap_edits_t delete_edits = ED[j];
            affine_gap_edits_t delete_edits_stored = delete_edits;
            delete_op = AFFINE_GAP_OPEN;
            if (C[j] + start_gap_cost < min) {
                min = C[j] + start_gap_cost;
                
                delete_edits = delete_edits_stored = E[j];
                delete_edits_stored.num_gap_opens++;
            }

            D[j] = min + extend_gap_cost;
            delete_edits_stored.num_gap_extensions++;
            ED[j] = delete_edits_stored;

            // Cost
            // c = min(DD[j], e, s + w(a, b))

            affine_gap_op current_op = delete_op;


            min = D[j];

            // Delete transition
            current_edits = delete_edits;

            if (e < min) {
                min = e;
                // Insert transition
                current_op = insert_op;
                current_edits = prev_char_edits;
            }

            bool both_separators = space_or_equivalent((int32_t)c1) && space_or_equivalent((int32_t)c2);

            bool is_transpose = false;
            size_t w = c1 != c2 && !both_separators ? mismatch_cost : match_cost;

            if (c1 != c2 && utf8_is_letter(utf8proc_category(c2)) && utf8_is_letter(utf8proc_category(c1)) && j < m && c2 == u1[j] && i < n && c1 == u2[i]) {
                w = transpose_cost;
                is_transpose = true;
            }

            if (s + w < min) {
                min = s + w;

                // Match/mismatch/transpose transition
                current_edits = prev_row_prev_char_edits;

                if ((c1 == c2 || both_separators) && !is_transpose) {
                    current_op = AFFINE_CHAR_MATCH;
                } else if (!is_transpose) {
                    current_op = AFFINE_CHAR_MISMATCH;
                } else if (is_transpose) {
                    current_op = AFFINE_TRANSPOSITION;
                }
            }

            if (current_op == AFFINE_CHAR_MATCH) {
                current_edits.num_matches++;
            } else if (current_op == AFFINE_CHAR_MISMATCH) {
                current_edits.num_mismatches++;
            } else if (current_op == AFFINE_GAP_EXTEND) {
                current_edits.num_gap_extensions++;
            } else if (current_op == AFFINE_GAP_OPEN) {
                current_edits.num_gap_opens++;
                current_edits.num_gap_extensions++;
            } else if (current_op == AFFINE_TRANSPOSITION) {
                current_edits.num_transpositions++;
            }

            if (min < min_cost) {
                op = current_op;
                min_cost = min;
                min_at = j;
            }

            c = min;
            s = C[j];
            C[j] = c;

            prev_char_edits = current_edits;
            prev_row_prev_char_edits = E[j];
            E[j] = prev_char_edits;

            // In the case of a transposition, duplicate costs for next character and advance by 2
            if (current_op == AFFINE_TRANSPOSITION) {
                E[j + 1] = E[j];
                C[j + 1] = C[j];
                j++;
            }
        }

        if (op == AFFINE_TRANSPOSITION) {
            i++;
        }

    }

    edits = E[m];
    free(C);
    free(D);
    free(E);
    free(ED);

    return edits;

}

affine_gap_edits_t affine_gap_distance_unicode(uint32_array *u1_array, uint32_array *u2_array) {
    return affine_gap_distance_unicode_costs(u1_array, u2_array, DEFAULT_AFFINE_GAP_OPEN_COST, DEFAULT_AFFINE_GAP_EXTEND_COST, DEFAULT_AFFINE_GAP_MATCH_COST, DEFAULT_AFFINE_GAP_MISMATCH_COST, DEFAULT_AFFINE_GAP_TRANSPOSE_COST);
}

affine_gap_edits_t affine_gap_distance_costs(const char *s1, const char *s2, size_t start_gap_cost, size_t extend_gap_cost, size_t match_cost, size_t mismatch_cost, size_t transpose_cost) {
    if (s1 == NULL || s2 == NULL) return NULL_AFFINE_GAP_EDITS;

    uint32_array *u1_array = unicode_codepoints(s1);
    if (u1_array == NULL) return NULL_AFFINE_GAP_EDITS;

    uint32_array *u2_array = unicode_codepoints(s2);

    if (u2_array == NULL) {
        uint32_array_destroy(u1_array);
        return NULL_AFFINE_GAP_EDITS;
    }

    affine_gap_edits_t edits = affine_gap_distance_unicode_costs(u1_array, u2_array, start_gap_cost, extend_gap_cost, match_cost, mismatch_cost, transpose_cost);

    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);

    return edits;
}


affine_gap_edits_t affine_gap_distance(const char *s1, const char *s2) {
    return affine_gap_distance_costs(s1, s2, DEFAULT_AFFINE_GAP_OPEN_COST, DEFAULT_AFFINE_GAP_EXTEND_COST, DEFAULT_AFFINE_GAP_MATCH_COST, DEFAULT_AFFINE_GAP_MISMATCH_COST, DEFAULT_AFFINE_GAP_TRANSPOSE_COST);
}


bool possible_abbreviation_unicode_with_edits(uint32_array *u1_array, uint32_array *u2_array, affine_gap_edits_t edits) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;
    if (len1 == 0 || len2 == 0) return false;

    size_t min_len = len1 < len2 ? len1 : len2;

    return edits.num_matches == min_len && u1_array->a[0] == u2_array->a[0];
}

inline bool possible_abbreviation_unicode(uint32_array *u1_array, uint32_array *u2_array) {
    affine_gap_edits_t edits = affine_gap_distance_unicode(u1_array, u2_array);

    ssize_t prefix_len = unicode_common_prefix(u1_array, u2_array);

    return prefix_len > 0 && possible_abbreviation_unicode_with_edits(u1_array, u2_array, edits);
}


bool possible_abbreviation_unicode_strict(uint32_array *u1_array, uint32_array *u2_array) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;
    if (len1 == 0 || len2 == 0) return false;

    size_t min_len = len1 < len2 ? len1 : len2;

    ssize_t prefix_len = unicode_common_prefix(u1_array, u2_array);
    if (prefix_len == min_len) return true;
    ssize_t suffix_len = unicode_common_suffix(u1_array, u2_array);
    return suffix_len > 0 && prefix_len > 0 && possible_abbreviation_unicode(u1_array, u2_array);
}

static bool possible_abbreviation_options(const char *s1, const char *s2, bool strict) {
    if (s1 == NULL || s2 == NULL) return false;

    uint32_array *u1_array = unicode_codepoints(s1);
    if (u1_array == NULL) return false;

    uint32_array *u2_array = unicode_codepoints(s2);

    if (u2_array == NULL) {
        uint32_array_destroy(u1_array);
        return false;
    }

    bool abbrev = false;
    if (!strict) {
        abbrev = possible_abbreviation_unicode(u1_array, u2_array);
    } else {
        abbrev = possible_abbreviation_unicode_strict(u1_array, u2_array);
    }

    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);

    return abbrev;
}

inline bool possible_abbreviation(const char *s1, const char *s2) {
    return possible_abbreviation_options(s1, s2, false);
}

inline bool possible_abbreviation_strict(const char *s1, const char *s2) {
    return possible_abbreviation_options(s1, s2, true);
}


ssize_t damerau_levenshtein_distance_unicode(uint32_array *u1_array, uint32_array *u2_array, size_t replace_cost) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    size_t num_bytes = (len1 + 1) * sizeof(size_t);

    size_t *column = malloc(num_bytes);
    if (column == NULL) {
        return -1.0;
    }

    for (size_t y = 1; y <= len1; y++) {
        column[y] = y;
    }

    size_t transpose_diag = 0;
    size_t last_diag = 0;

    for (size_t x = 1; x <= len2; x++) {
        column[0] = x;
        last_diag = x - 1;

        for (size_t y = 1; y <= len1; y++) {
            size_t old_diag = column[y];
            size_t cost = (u1[y - 1] == u2[x - 1] ? 0 : 1);

            size_t v1 = column[y] + 1;
            size_t v2 = column[y - 1] + 1;
            size_t v3 = last_diag + cost;

            size_t min = v1;
            if (v2 < min) min = v2;
            if (v3 < min) min = v3;

            if (x > 1 && y > 1 && u1[y - 1] == u2[x - 2] && u1[y - 2] == u2[x - 1]) {
                size_t v4 = transpose_diag;
                if (v4 < min) min = v4;
            }

            column[y] = min;

            last_diag = old_diag;
            transpose_diag = last_diag;
        }
    }

    size_t dist = column[len1];
    free(column);
    return (ssize_t)dist;
}

ssize_t damerau_levenshtein_distance_replace_cost(const char *s1, const char *s2, size_t replace_cost) {
    if (s1 == NULL || s2 == NULL) return -1;

    uint32_array *u1_array = unicode_codepoints(s1);
    if (u1_array == NULL) return -1.0;

    uint32_array *u2_array = unicode_codepoints(s2);

    if (u2_array == NULL) {
        uint32_array_destroy(u1_array);
        return -1.0;
    }

    ssize_t lev = damerau_levenshtein_distance_unicode(u1_array, u2_array, replace_cost);

    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);
    return lev;
}

ssize_t damerau_levenshtein_distance(const char *s1, const char *s2) {
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

    uint8_t *u1_matches = calloc(len1, sizeof(uint8_t));
    uint8_t *u2_matches = calloc(len2, sizeof(uint8_t));

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

    uint32_array *u1_array = unicode_codepoints(s1);
    if (u1_array == NULL) return -1.0;

    uint32_array *u2_array = unicode_codepoints(s2);

    if (u2_array == NULL) {
        uint32_array_destroy(u1_array);
        return -1.0;
    }

    double jaro = jaro_distance_unicode(u1_array, u2_array);
    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);
    return jaro;
}

#define MAX_JARO_WINKLER_PREFIX 4

double jaro_winkler_distance_unicode_prefix_threshold(uint32_array *u1_array, uint32_array *u2_array, double prefix_scale, double bonus_threshold) {
    double jaro = jaro_distance_unicode(u1_array, u2_array);

    double j;

    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;

    size_t m = len1 < len2 ? len1 : len2;

    size_t shared_prefix = 0;
    for (size_t i = 0; i < m; i++) {
        if (u1[i] != u2[i]) break;
        shared_prefix++;
        if (shared_prefix > MAX_JARO_WINKLER_PREFIX) {
            shared_prefix = MAX_JARO_WINKLER_PREFIX;
            break;
        }
    }

    double jaro_winkler = jaro;

    if (jaro >= bonus_threshold) {
        jaro_winkler += (1.0 - jaro) * shared_prefix * prefix_scale;
    }

    return jaro_winkler > 1.0 ? 1.0 : jaro_winkler;
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

    double jaro_winkler = jaro_winkler_distance_unicode_prefix_threshold(u1_array, u2_array, prefix_scale, bonus_threshold);

    uint32_array_destroy(u1_array);
    uint32_array_destroy(u2_array);

    return jaro_winkler;
}

inline double jaro_winkler_distance(const char *s1, const char *s2) {
    return jaro_winkler_distance_prefix_threshold(s1, s2, DEFAULT_JARO_WINKLER_PREFIX_SCALE, DEFAULT_JARO_WINKLER_BONUS_THRESHOLD);
}

inline double jaro_winkler_distance_unicode(uint32_array *u1_array, uint32_array *u2_array) {
    return jaro_winkler_distance_unicode_prefix_threshold(u1_array, u2_array, DEFAULT_JARO_WINKLER_PREFIX_SCALE, DEFAULT_JARO_WINKLER_BONUS_THRESHOLD);
}

phrase_array *multi_word_token_alignments(const char *s1, token_array *tokens1, const char *s2, token_array *tokens2) {
    if (s1 == NULL || tokens1 == NULL || s2 == NULL || tokens2 == NULL) {
        return NULL;
    }

    size_t len1 = tokens1->n;
    size_t len2 = tokens2->n;
    if (len1 == 0 || len2 == 0 || len1 == len2) return NULL;

    if (len1 > len2) {
        const char *tmp_s = s1;
        s1 = s2;
        s2 = tmp_s;

        token_array *tmp_t = tokens1;
        tokens1 = tokens2;
        tokens2 = tmp_t;

        size_t tmp_l = len1;
        len1 = len2;
        len2 = tmp_l;
    }

    phrase_array *alignments = NULL;

    token_t *t1 = tokens1->a;
    token_t *t2 = tokens2->a;

    ssize_t phrase_start = -1;
    ssize_t phrase_token_pos = -1;

    uint8_t *ptr1 = (uint8_t *)s1;
    uint8_t *ptr2 = (uint8_t *)s2;

    int32_t c1;
    ssize_t c1_len;

    for (size_t i = 0; i < len1; i++) {
        token_t ti = t1[i];

        c1_len = utf8proc_iterate(ptr1 + ti.offset, ti.len, &c1);
        if (c1_len <= 0 || c1 == 0) {
            break;
        }

        if (!(is_word_token(ti.type) || is_numeric_token(ti.type)) || is_ideographic(ti.type)) {
            phrase_token_pos = -1;
            continue;
        }

        size_t ti_pos = 0;

        for (size_t j = 0; j < len2; j++) {
            token_t tj = t2[j];

            if (utf8_compare_len_case_insensitive(ptr1 + ti.offset + ti_pos, ptr2 + tj.offset, tj.len) == 0) {
                ti_pos += tj.len;
                if (phrase_start < 0) {
                    phrase_start = j;
                    phrase_token_pos = 0;
                }
                phrase_token_pos++;
            } else {
                phrase_token_pos = -1;
                phrase_start = -1;
                ti_pos = 0;
                continue;
            }

            if (ti_pos == ti.len && j - phrase_start > 0) {
                phrase_t phrase = (phrase_t){phrase_start, j - phrase_start + 1, i};
                // got alignment
                if (alignments == NULL) {
                    alignments = phrase_array_new();
                }

                phrase_array_push(alignments, phrase);

                ti_pos = 0;
                phrase_token_pos = -1;
                phrase_start = -1;
            }
        }

    }

    return alignments;
}

