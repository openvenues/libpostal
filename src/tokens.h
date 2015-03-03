

#ifndef TOKENS_H
#define TOKENS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "klib/khash.h"
#include "sds/sds.h"
#include "collections.h"
#include "string_utils.h"
#include "token_types.h"
#include "vector.h"

typedef struct token {
    size_t offset;
    size_t len;
    uint16_t type;
    uint64_t src_position;
} token_t;

VECTOR_INIT(token_array, token_t)

typedef struct tokenized_string {
    char_array *str;
    token_array *tokens;
} tokenized_string_t;

tokenized_string_t *tokenized_string_new(void);
void tokenized_string_add_token(tokenized_string_t *self, const char *src, size_t len, uint16_t token_type, uint64_t src_position);
char *tokenized_string_get_token(tokenized_string_t *self, uint64_t index);
void tokenized_string_destroy(tokenized_string_t *self);


#ifdef __cplusplus
}
#endif

#endif
