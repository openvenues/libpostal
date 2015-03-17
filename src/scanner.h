#ifndef SCANNER_H
#define SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "token_types.h"
#include "tokens.h"

typedef struct scanner {
    unsigned char *src, *cursor, *start, *end;
} scanner_t;

int scan_token(scanner_t *s);

inline scanner_t scanner_from_string(const char *input);

tokenized_string_t *tokenize(const char *str);


#ifdef __cplusplus
}
#endif

#endif
