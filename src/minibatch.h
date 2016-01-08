#ifndef MINIBATCH_H
#define MINIBATCH_H

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"
#include "features.h"
#include "sparse_matrix.h"
#include "trie.h"
#include "trie_utils.h"
#include "vector_math.h"


bool count_features_minibatch(khash_t(str_double) *feature_counts, feature_count_array *minibatch, bool unique);
bool count_labels_minibatch(khash_t(str_uint32) *label_counts, cstring_array *labels);

trie_t *select_features_threshold(khash_t(str_double) *feature_counts, double threshold);
khash_t(str_uint32) *select_labels_threshold(khash_t(str_uint32) *label_counts, uint32_t threshold);

sparse_matrix_t *feature_matrix(trie_t *feature_ids, feature_count_array *feature_counts);
sparse_matrix_t *feature_vector(trie_t *feature_ids, khash_t(str_double) *feature_counts);
uint32_array *label_vector(khash_t(str_uint32) *label_ids, cstring_array *labels);


#endif