#ifndef ACRONYMS_H
#define ACRONYMS_H

#include <stdio.h>
#include <stdlib.h>

#include "address_dictionary.h"
#include "collections.h"
#include "tokens.h"
#include "token_types.h"

bool stopword_positions(uint32_array *stopwords_array, const char *str, token_array *tokens, size_t num_languages, char **languages);
bool existing_acronym_phrase_positions(uint32_array *existing_acronyms_array, const char *str, token_array *token_array, size_t num_languages, char **languages);

phrase_array *acronym_token_alignments(const char *s1, token_array *tokens1, const char *s2, token_array *tokens2, size_t num_languages, char **languages);


#endif