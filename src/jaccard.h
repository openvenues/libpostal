#ifndef JACCARD_H
#define JACCARD_H

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"

double jaccard_similarity(khash_t(str_set) *s1, khash_t(str_set) *s2);
double jaccard_similarity_string_arrays(size_t num_strings1, char **strings1, size_t num_strings2, char **strings2);

#endif