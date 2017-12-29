#ifndef DEDUPE_H
#define DEDUPE_H

#include <stdlib.h>
#include <stdio.h>

#include "libpostal.h"
#include "string_utils.h"

libpostal_duplicate_status_t is_name_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_street_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_house_number_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_po_box_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_unit_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_floor_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_postal_code_duplicate(char *value1, char *value2, libpostal_duplicate_options_t options);
libpostal_duplicate_status_t is_toponym_duplicate(size_t num_components1, char **labels1, char **values1, size_t num_components2, char **labels2, char **values2, libpostal_duplicate_options_t options);

libpostal_fuzzy_duplicate_status_t is_name_duplicate_fuzzy(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options);
libpostal_fuzzy_duplicate_status_t is_street_duplicate_fuzzy(size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options);


#endif