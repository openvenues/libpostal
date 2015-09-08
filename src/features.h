#ifndef FEATURES_H
#define FEATURES_H

#include <stdlib.h>
#include <stdarg.h>
#include "collections.h"
#include "string_utils.h"

#define FEATURE_SEPARATOR_CHAR "|"

// Add feature to array

void feature_array_add(cstring_array *features, size_t count, ...);

// Add feature count to dictionary

bool feature_counts_update(khash_t(str_uint32) *features, char *feature, int count); 

#endif