/*
averaged_perceptron.h
---------------------

The averaged perceptron is a simple, efficient and effective method for
training sequence models.

The averaged perceptron is a linear model, meaning the score for a given class
is the dot product of weights and the feature values.

This implementation of the averaged perceptron uses a trie data structure to
store the mapping from features to indices, which can be quite memory efficient
as opposed to a hash table and allows us to store millions of features with
very little memory.

The weights are stored as a sparse matrix in compressed sparse row format
(see sparse_matrix.h)
*/
#ifndef AVERAGED_PERCEPTRON_H
#define AVERAGED_PERCEPTRON_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "collections.h"
#include "sparse_matrix.h"
#include "trie.h"

typedef struct averaged_perceptron {
    uint32_t num_features;
    uint32_t num_classes;
    trie_t *features;
    cstring_array *classes;
    sparse_matrix_t *weights;
    double_array *scores;
} averaged_perceptron_t;

averaged_perceptron_t *averaged_perceptron_read(FILE *f);
averaged_perceptron_t *averaged_perceptron_load(char *filename);

uint32_t averaged_perceptron_predict(averaged_perceptron_t *self, cstring_array *features);
uint32_t averaged_perceptron_predict_counts(averaged_perceptron_t *self, khash_t(str_uint32) *feature_counts);

double_array *averaged_perceptron_predict_scores(averaged_perceptron_t *self, cstring_array *features);
double_array *averaged_perceptron_predict_scores_counts(averaged_perceptron_t *self, khash_t(str_uint32) *feature_counts);

bool averaged_perceptron_write(averaged_perceptron_t *self, FILE *f);
bool averaged_perceptron_save(averaged_perceptron_t *self, char *filename);

averaged_perceptron_t *averaged_perceptron_read(FILE *f);
averaged_perceptron_t *averaged_perceptron_load(char *filename);

void averaged_perceptron_destroy(averaged_perceptron_t *self);


#endif