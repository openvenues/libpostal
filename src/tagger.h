#ifndef TAGGER_H
#define TAGGER_H

#include "string_utils.h"
#include "tokens.h"

// Arguments:                           tagger, context, tokenized str,       index
typedef bool (*tagger_feature_function)(void *, void *, tokenized_string_t *, uint32_t);

#endif
