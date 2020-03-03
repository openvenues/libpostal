#ifndef LANGUAGE_FEATURES_H
#define LANGUAGE_FEATURES_H

#include <stdlib.h>

#include "collections.h"
#include "string_utils.h"
#include "tokens.h"
#include "address_dictionary.h"


void language_classifier_normalize_token(char_array *array, char *str, token_t token);

khash_t(str_double) *extract_language_features(address_dictionary_t *address_dict, char *str, char *country, token_array *tokens, char_array *feature_array);

#include "libpostal.h"

char *language_classifier_normalize_string(libpostal_t *instance, char *str);

#endif