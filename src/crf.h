/*
crf.h
---------------------------------------------------------------
A linear-chain CRF tagger tries to find the best labeling
for a sequence. The feature function can use the current token,
surrounding tokens and n (typically n=2) previous predictions
to predict the current transition matrix.

*/

#ifndef CRF_H
#define CRF_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "collections.h"
#include "crf_context.h"
#include "matrix.h"
#include "sparse_matrix.h"
#include "tagger.h"
#include "trie.h"

typedef struct crf {
    uint32_t num_classes;
    cstring_array *classes;
    trie_t *state_features;
    sparse_matrix_t *weights;
    trie_t *state_trans_features;
    sparse_matrix_t *state_trans_weights;
    double_matrix_t *trans_weights;
    uint32_array *viterbi;
    crf_context_t *context;
} crf_t;

bool crf_tagger_predict(crf_t *model, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *labels, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features);

bool crf_tagger_score(crf_t *self, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features);
bool crf_tagger_score_viterbi(crf_t *self, void *tagger, void *tagger_context, cstring_array *features, cstring_array *prev_tag_features, tagger_feature_function feature_function, tokenized_string_t *tokenized, double *score, bool print_features);

bool crf_tagger_predict(crf_t *self, void *tagger, void *context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *labels, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features);

bool crf_write(crf_t *self, FILE *f);
bool crf_save(crf_t *self, char *filename);

crf_t *crf_read(FILE *f);
crf_t *crf_load(char *filename);

void crf_destroy(crf_t *self);

#endif