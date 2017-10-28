#ifndef STRING_SIMILARITY_H
#define STRING_SIMILARITY_H

#include <stdio.h>
#include <stdlib.h>

#include "collections.h"

#define DEFAULT_JARO_WINKLER_PREFIX_SCALE 0.1
#define DEFAULT_JARO_WINKLER_BONUS_THRESHOLD 0.7

ssize_t damerau_levenshtein_distance(const char *s1, const char *s2);
ssize_t damerau_levenshtein_distance_unicode(uint32_array *u1_array, uint32_array *u2_array, size_t replace_cost);
ssize_t damerau_levenshtein_distance_replace_cost(const char *s1, const char *s2, size_t replace_cost);

double jaro_distance(const char *s1, const char *s2);
double jaro_distance_unicode(uint32_array *u1_array, uint32_array *u2_array);
double jaro_winkler_distance_prefix_threshold(const char *s1, const char *s2, double prefix_scale, double bonus_threshold);
double jaro_winkler_distance_unicode_prefix_threshold(uint32_array *u1_array, uint32_array *u2_array, double prefix_scale, double bonus_threshold);
double jaro_winkler_distance(const char *s1, const char *s2);
double jaro_winkler_distance_unicode(uint32_array *u1_array, uint32_array *u2_array);


#endif
