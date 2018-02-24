#ifndef SOFT_TFIDF_H
#define SOFT_TFIDF_H

#include <stdlib.h>
#include "collections.h"
#include "libpostal.h"
#include "trie_search.h"

/*
This is a variant of Soft-TFIDF as described in:

Cohen, Ravikumar, and Fienberg. A comparison of string distance
metrics for name-matching tasks. (2003)
https://www.cs.cmu.edu/~wcohen/postscript/ijcai-ws-2003.pdf

Soft TFIDF is a hybrid similarity function for strings, typically names,
which combines both global statistics (TF-IDF) and a local similarity
function (e.g. Jaro-Winkler, which the authors suggest performs best).

Given two strings, s1 and s2, each token t1 in s1 is matched with its most
similar counterpart t2 in s2 according to the local distance function.

The Soft-TFIDF similarity is then the dot product of the max token
similarities and the cosine similarity of the TF-IDF vectors for all tokens
if the max similarity is >= a given threshold theta.

This version is a modified Soft-TFIDF. Jaro-Winkler is used as the secondary
distance metric. However, the defintion of two tokens being "similar" is
defined as either:

1. Jaro-Winkler distance >= theta
2. Damerau-Levenshtein edit distance <= max_edit_distance
3. Affine gap edit counts indicate a possible abbreviation (# matches == min(len1, len2))
*/

typedef struct soft_tfidf_options {
    double jaro_winkler_min;
    size_t jaro_winkler_min_length;
    size_t damerau_levenshtein_max;
    size_t damerau_levenshtein_min_length;
    bool possible_affine_gap_abbreviations;
    size_t strict_abbreviation_min_length;
    double strict_abbreviation_sim;
} soft_tfidf_options_t;

soft_tfidf_options_t soft_tfidf_default_options(void);

double soft_tfidf_similarity_with_phrases_and_acronyms(size_t num_tokens1, char **tokens1, double *token_scores1, phrase_array *phrases1, uint32_array *ordinal_suffixes1, size_t num_tokens2, char **tokens2, double *token_scores2, phrase_array *phrases2, uint32_array *ordinal_suffixes2, phrase_array *acronym_alignments, phrase_array *multi_word_alignments, soft_tfidf_options_t options, size_t *num_matches);
double soft_tfidf_similarity(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, soft_tfidf_options_t options, size_t *num_matches);

#endif