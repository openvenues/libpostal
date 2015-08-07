#ifndef FEATURES_H
#define FEATURES_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include "string_utils.h"

#define FEATURE_SEPARATOR_CHAR "|"

void feature_array_add(cstring_array *features, size_t count, ...);


#ifdef __cplusplus
}
#endif

#endif