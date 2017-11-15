#ifndef CRF_AVERAGED_PERCEPTRON_TRAINER_H
#define CRF_AVERAGED_PERCEPTRON_TRAINER_H

#include <stdio.h>
#include <stdlib.h>

#include "averaged_perceptron_trainer.h"
#include "crf.h"
#include "crf_trainer.h"
#include "collections.h"
#include "string_utils.h"
#include "tokens.h"
#include "trie.h"
#include "trie_utils.h"

typedef union tag_bigram {
    uint64_t value;
    struct {
        uint32_t prev_class_id:32;
        uint32_t class_id:32;
    };
} tag_bigram_t;

KHASH_MAP_INIT_INT64(prev_tag_class_weights, class_weight_t)

KHASH_MAP_INIT_INT(feature_prev_tag_class_weights, khash_t(prev_tag_class_weights) *)

typedef struct crf_averaged_perceptron_trainer {
    crf_trainer_t *base_trainer;
    uint64_t num_updates;
    uint64_t num_errors;
    uint32_t iterations;
    uint64_t min_updates;
    // {feature_id => {class_id => class_weight_t}}
    khash_t(feature_class_weights) *weights;
    khash_t(feature_prev_tag_class_weights) *prev_tag_weights;
    khash_t(prev_tag_class_weights) *trans_weights;
    uint64_array *update_counts;
    uint64_array *prev_tag_update_counts;
    cstring_array *sequence_features;
    uint32_array *sequence_features_indptr;
    cstring_array *sequence_prev_tag_features;
    uint32_array *sequence_prev_tag_features_indptr;
    uint32_array *label_ids;
    uint32_array *viterbi;
} crf_averaged_perceptron_trainer_t;

crf_averaged_perceptron_trainer_t *crf_averaged_perceptron_trainer_new(size_t num_classes, size_t min_updates);

uint32_t crf_averaged_perceptron_trainer_predict(crf_averaged_perceptron_trainer_t *self, cstring_array *features);

bool crf_averaged_perceptron_trainer_train_example(crf_averaged_perceptron_trainer_t *self, 
                                                   void *tagger,
                                                   void *context,
                                                   cstring_array *features,
                                                   cstring_array *prev_tag_features,
                                                   tagger_feature_function feature_function,
                                                   tokenized_string_t *tokenized,
                                                   cstring_array *labels
                                                   );

crf_t *crf_averaged_perceptron_trainer_finalize(crf_averaged_perceptron_trainer_t *self);

void crf_averaged_perceptron_trainer_destroy(crf_averaged_perceptron_trainer_t *self);


#endif
