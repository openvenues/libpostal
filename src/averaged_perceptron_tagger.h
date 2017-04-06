/*
averaged_perceptron_tagger.h
----------------------------

An averaged perceptron tagger is a greedy sequence labeling
algorithm which uses features of the current token, surrounding
tokens and n (typically n=2) previous predictions to predict
the current value.

*/

#ifndef AVERAGED_PERCEPTRON_TAGGER_H
#define AVERAGED_PERCEPTRON_TAGGER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "averaged_perceptron.h"
#include "features.h"
#include "tagger.h"
#include "tokens.h"

#define START "START"
#define START2 "START2"

bool averaged_perceptron_tagger_predict(averaged_perceptron_t *model, void *tagger, void *context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *prev2_tag_features, cstring_array *labels, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features);

#endif