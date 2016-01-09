#ifndef LANGUAGE_FEATURES_H
#define LANGUAGE_FEATURES_H

#include <stdlib.h>

#include "collections.h"
#include "string_utils.h"
#include "tokens.h"


char *language_classifier_normalize_string(char *str);
void language_classifier_normalize_token(char_array *array, char *str, token_t token);

khash_t(str_double) *extract_language_features(char *str, char *country, token_array *tokens, char_array *feature_array);

#endif