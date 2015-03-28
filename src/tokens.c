#include "tokens.h"


tokenized_string_t *tokenized_string_new(void) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = cstring_array_new();
    self->tokens = token_array_new();

    return self;
}


void tokenized_string_add_token(tokenized_string_t *self, const char *src, size_t len, uint16_t token_type, size_t position) {
    char *ptr = (char *) (src + position);

    cstring_array_add_string_len(self->str, ptr, len);

    token_t token = (token_t){position, len, token_type};
    token_array_push(self->tokens, token);

}

tokenized_string_t *tokenized_string_from_tokens(char *src, token_array *tokens) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = cstring_array_new_size(strlen(src));
    self->tokens = tokens;

    token_t token;

    for (int i = 0; i < tokens->n; i++) {
        token = tokens->a[i];
        cstring_array_add_string_len(self->str, src + token.offset, token.len);
    }
    return self;
}


char *tokenized_string_get_token(tokenized_string_t *self, uint32_t index) {
    if (index < self->tokens->n) {
        return cstring_array_get_token(self->str, index);
    } else {
        return NULL;
    }
}

void tokenized_string_destroy(tokenized_string_t *self) {
    if (!self)
        return;
    if (self->str)
        cstring_array_destroy(self->str);
    if (self->tokens)
        token_array_destroy(self->tokens);
    free(self);
}
