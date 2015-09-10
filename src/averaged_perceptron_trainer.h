/*
averaged_perceptron_trainer.h
-----------------------------

Trainer for a generic averaged perceptron model.

The averaged perceptron uses a simple online error-driven
learning algorithm. Given some features and the true label,
it predicts the expected label under the current weights. If
it guess correctly, there's nothing to do and it moves
on to the next example. If it predicted the wrong answer, it
makes the following updates to its weights:

weights[feature][predicted] -= 1.0
weights[feature][actual] += 1.0

This seems overly simplistic, and it is. This is the regular
perceptron update rule. On the more difficult cases, this model
would tend to overfit by spending a lot of time fiddling with the
weights for the few cases it got wrong and building the whole model
around those few cases. The averaged perceptron is one way to account
for this and build a more robust model. 


Paper: [Collins, 2002] Discriminative Training Methods for Hidden Markov Models: 
                       Theory and Experiments with Perceptron Algorithms

Link: http://www.cs.columbia.edu/~mcollins/papers/tagperc.pdf
*/
#ifndef AVERAGED_PERCEPTRON_TRAINER_H
#define AVERAGED_PERCEPTRON_TRAINER_H

#include <stdio.h>
#include <stdlib.h>

#include "averaged_perceptron.h"
#include "collections.h"
#include "string_utils.h"
#include "trie.h"

typedef struct class_weight {
    double value;
    double total;
    uint64_t last_updated;
} class_weight_t;

#define NULL_WEIGHT (class_weight_t){0.0, 0.0, 0}

KHASH_MAP_INIT_INT(class_weights, class_weight_t)

KHASH_MAP_INIT_INT(feature_class_weights, khash_t(class_weights) *) 

typedef struct averaged_perceptron_trainer {
    uint32_t num_features;
    uint32_t num_classes;
    uint64_t num_updates;
    uint64_t num_errors;
    trie_t *features;
    khash_t(str_uint32) *classes;
    cstring_array *class_strings;
    // {feature_id => {class_id => class_weight_t}}
    khash_t(feature_class_weights) *weights;
    double_array *scores;
} averaged_perceptron_trainer_t;

averaged_perceptron_trainer_t *averaged_perceptron_trainer_new(void);

uint32_t averaged_perceptron_trainer_predict(averaged_perceptron_trainer_t *self, cstring_array *features);
bool averaged_perceptron_trainer_train_example(averaged_perceptron_trainer_t *trainer, cstring_array *features, char *label);

averaged_perceptron_t *averaged_perceptron_trainer_finalize(averaged_perceptron_trainer_t *self);

void averaged_perceptron_trainer_destroy(averaged_perceptron_trainer_t *self);

#endif
