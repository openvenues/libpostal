#ifndef STRING_SIMILARITY_H
#define STRING_SIMILARITY_H

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"
#include "trie_search.h"

#define DEFAULT_AFFINE_GAP_OPEN_COST 3
#define DEFAULT_AFFINE_GAP_EXTEND_COST 2
#define DEFAULT_AFFINE_GAP_MATCH_COST 0
#define DEFAULT_AFFINE_GAP_MISMATCH_COST 6
#define DEFAULT_AFFINE_GAP_TRANSPOSE_COST 4

typedef struct affine_gap_edits {
    size_t num_matches;
    size_t num_mismatches;
    size_t num_transpositions;
    size_t num_gap_opens;
    size_t num_gap_extensions;
} affine_gap_edits_t;

affine_gap_edits_t affine_gap_distance(const char *s1, const char *s2);
affine_gap_edits_t affine_gap_distance_unicode(uint32_array *u1_array, uint32_array *u2_array);

bool possible_abbreviation(const char *s1, const char *s2);
bool possible_abbreviation_strict(const char *s1, const char *s2);
bool possible_abbreviation_unicode(uint32_array *u1_array, uint32_array *u2_array);
bool possible_abbreviation_unicode_strict(uint32_array *u1_array, uint32_array *u2_array);
bool possible_abbreviation_unicode_with_edits(uint32_array *u1_array, uint32_array *u2_array, affine_gap_edits_t edits);

ssize_t damerau_levenshtein_distance(const char *s1, const char *s2);
ssize_t damerau_levenshtein_distance_unicode(uint32_array *u1_array, uint32_array *u2_array, size_t replace_cost);
ssize_t damerau_levenshtein_distance_replace_cost(const char *s1, const char *s2, size_t replace_cost);

#define DEFAULT_JARO_WINKLER_PREFIX_SCALE 0.1
#define DEFAULT_JARO_WINKLER_BONUS_THRESHOLD 0.7

double jaro_distance(const char *s1, const char *s2);
double jaro_distance_unicode(uint32_array *u1_array, uint32_array *u2_array);
double jaro_winkler_distance_prefix_threshold(const char *s1, const char *s2, double prefix_scale, double bonus_threshold);
double jaro_winkler_distance_unicode_prefix_threshold(uint32_array *u1_array, uint32_array *u2_array, double prefix_scale, double bonus_threshold);
double jaro_winkler_distance(const char *s1, const char *s2);
double jaro_winkler_distance_unicode(uint32_array *u1_array, uint32_array *u2_array);

phrase_array *multi_word_token_alignments(const char *s1, token_array *tokens1, const char *s2, token_array *tokens2);

#endif
