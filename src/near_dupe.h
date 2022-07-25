
#ifndef NEAR_DUPE_H
#define NEAR_DUPE_H

#include <stdlib.h>
#include <stdio.h>

#include "libpostal.h"
#include "string_utils.h"

cstring_array *name_word_hashes(char *name, libpostal_normalize_options_t normalize_options);
cstring_array *near_dupe_hashes(size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options);
cstring_array *near_dupe_hashes_languages(size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options, size_t num_languages, char **languages);

#endif