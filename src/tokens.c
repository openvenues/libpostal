#include "tokens.h"


tokenized_string_t *tokenized_string_new(void) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = char_array_new();
    self->tokens = token_array_new();

    return self;
}

void tokenized_string_add_token(tokenized_string_t *self, const char *src, size_t len, uint16_t token_type, uint64_t position) {
    char *ptr = (char *) (src + position);
    size_t offset = self->str->n;

    contiguous_string_array_add_string_len(self->str, ptr, len);

    token_t token = (token_t){offset, len, token_type, position};
    token_array_push(self->tokens, token);

}

char *tokenized_string_get_token(tokenized_string_t *self, uint64_t index) {
    if (index < self->tokens->n) {
        uint64_t i = self->tokens->a[index].offset;
        return (char *)self->str->a + i;
    } else {
        return NULL;
    }
}

void tokenized_string_destroy(tokenized_string_t *self) {
    if (!self)
        return;
    if (self->str)
        char_array_destroy(self->str);
    if (self->tokens)
        token_array_destroy(self->tokens);
    free(self);
}