#ifndef LANGUAGE_CLASSIFIER_IO_H
#define LANGUAGE_CLASSIFIER_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "collections.h"
#include "features.h"
#include "file_utils.h"
#include "language_classifier.h"
#include "scanner.h"
#include "string_utils.h"

#define AMBIGUOUS_LANGUAGE "xxx"
#define UNKNOWN_LANGUAGE "unk"

#define LANGUAGE_CLASSIFIER_DEFAULT_BATCH_SIZE 1000

enum language_classifier_training_data_fields {
    LANGUAGE_CLASSIFIER_FIELD_LANGUAGE,
    LANGUAGE_CLASSIFIER_FIELD_COUNTRY,
    LANGUAGE_CLASSIFIER_FIELD_ADDRESS,
    LANGUAGE_CLASSIFIER_FILE_NUM_TOKENS
};

typedef struct language_classifier_data_set {
    FILE *f;
    token_array *tokens;
    char_array *feature_array;
    char_array *address;
    char_array *language;
    char_array *country;
} language_classifier_data_set_t;

typedef struct language_classifier_minibatch {
    feature_count_array *features;
    cstring_array *labels;
} language_classifier_minibatch_t;

language_classifier_data_set_t *language_classifier_data_set_init(char *filename);
bool language_classifier_data_set_next(language_classifier_data_set_t *self);
void language_classifier_data_set_destroy(language_classifier_data_set_t *self);

language_classifier_minibatch_t *language_classifier_data_set_get_minibatch_with_size(language_classifier_data_set_t *self, khash_t(str_uint32) *labels, size_t batch_size);
language_classifier_minibatch_t *language_classifier_data_set_get_minibatch(language_classifier_data_set_t *self, khash_t(str_uint32) *labels);
void language_classifier_minibatch_destroy(language_classifier_minibatch_t *self);

#endif