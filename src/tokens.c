#include "tokens.h"
#include "strndup.h"


tokenized_string_t *tokenized_string_new(void) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = NULL;
    self->strings = cstring_array_new();
    self->tokens = token_array_new();
    return self;
}

tokenized_string_t *tokenized_string_new_size(size_t len, size_t num_tokens) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = NULL;
    self->strings = cstring_array_new_size(len);
    self->tokens = token_array_new_size(len);

    return self;
}

inline tokenized_string_t *tokenized_string_new_from_str_size(char *src, size_t len, size_t num_tokens) {
    tokenized_string_t *self = tokenized_string_new_size(len, num_tokens);
    self->str = strndup(src, len);
    if (self->str == NULL) {
        tokenized_string_destroy(self);
        return NULL;
    }
    return self;
}


void tokenized_string_add_token(tokenized_string_t *self, const char *src, size_t len, uint16_t token_type, size_t position) {
    char *ptr = (char *) (src + position);

    cstring_array_add_string_len(self->strings, ptr, len);

    token_t token = (token_t){position, len, token_type};
    token_array_push(self->tokens, token);

}

tokenized_string_t *tokenized_string_from_tokens(char *src, token_array *tokens, bool copy_tokens) {
    tokenized_string_t *self = malloc(sizeof(tokenized_string_t));
    self->str = strdup(src);
    if (self->str == NULL) {
        tokenized_string_destroy(self);
        return NULL;
    }
    self->strings = cstring_array_new_size(strlen(src) + tokens->n);
    if (copy_tokens) {
        self->tokens = token_array_new_copy(tokens, tokens->n);
    } else {
        self->tokens = tokens;
    }

    token_t token;

    for (size_t i = 0; i < tokens->n; i++) {
        token = tokens->a[i];
        cstring_array_add_string_len(self->strings, src + token.offset, token.len);
    }
    return self;
}


char *tokenized_string_get_token(tokenized_string_t *self, uint32_t index) {
    if (index < self->tokens->n) {
        return cstring_array_get_string(self->strings, index);
    } else {
        return NULL;
    }
}

void tokenized_string_destroy(tokenized_string_t *self) {
    if (self == NULL) return;

    if (self->str != NULL) {
        free(self->str);
    }

    if (self->strings != NULL) {
        cstring_array_destroy(self->strings);
    }

    if (self->tokens != NULL) {
        token_array_destroy(self->tokens);
    }

    free(self);
}
