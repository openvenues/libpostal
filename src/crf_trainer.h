#ifndef CRF_TRAINER_H
#define CRF_TRAINER_H

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"
#include "crf_context.h"
#include "string_utils.h"
#include "tokens.h"
#include "trie.h"
#include "trie_utils.h"

typedef struct crf_trainer {
    uint32_t num_classes;
    khash_t(str_uint32) *features;
    khash_t(str_uint32) *prev_tag_features;
    khash_t(str_uint32) *classes;
    cstring_array *class_strings;
    crf_context_t *context;
} crf_trainer_t;


crf_trainer_t *crf_trainer_new(size_t num_classes);
void crf_trainer_destroy(crf_trainer_t *self);

bool crf_trainer_get_class_id(crf_trainer_t *self, char *class_name, uint32_t *class_id, bool add_if_missing);
bool crf_trainer_hash_feature_to_id(crf_trainer_t *self, char *feature, uint32_t *feature_id);
bool crf_trainer_hash_feature_to_id_exists(crf_trainer_t *self, char *feature, uint32_t *feature_id, bool *exists);

bool crf_trainer_hash_prev_tag_feature_to_id(crf_trainer_t *self, char *feature, uint32_t *feature_id);
bool crf_trainer_hash_prev_tag_feature_to_id_exists(crf_trainer_t *self, char *feature, uint32_t *feature_id, bool *exists);

bool crf_trainer_get_feature_id(crf_trainer_t *self, char *feature, uint32_t *feature_id);
bool crf_trainer_get_prev_tag_feature_id(crf_trainer_t *self, char *feature, uint32_t *feature_id);

#endif