
#ifndef NEAR_DUPE_H
#define NEAR_DUPE_H

#include <stdlib.h>
#include <stdio.h>

#include "string_utils.h"
#include "libpostal.h"

cstring_array *near_dupe_hashes(libpostal_t *instance, size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options);
cstring_array *near_dupe_hashes_languages(libpostal_t *instance, size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options, size_t num_languages, char **languages);

#endif