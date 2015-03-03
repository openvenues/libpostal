

#ifndef SCANNER_H
#define SCANNER_H


#ifdef __cplusplus
extern "C" {
#endif


#include "token_types.h"
#include "tokens.h"

tokenized_string_t *tokenize(const char *str);


#ifdef __cplusplus
}
#endif

#endif