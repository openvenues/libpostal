#ifndef TAGGER_H
#define TAGGER_H

#include "string_utils.h"
#include "tokens.h"
#include "address_dictionary.h"

// Arguments:                           dictionary,             tagger, context, tokenized str,       index
typedef bool (*tagger_feature_function)(address_dictionary_t *, void *, void *, tokenized_string_t *, uint32_t);

#endif
