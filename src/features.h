#ifndef FEATURES_H
#define FEATURES_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include "string_utils.h"

#define FEATURE_SEPARATOR_CHAR "|"

void feature_array_add(char_array *features, int num_args, ...);


#ifdef __cplusplus
}
#endif

#endif