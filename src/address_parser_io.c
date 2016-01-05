#include "address_parser_io.h"

address_parser_data_set_t *address_parser_data_set_init(char *filename) {
    address_parser_data_set_t *data_set = malloc(sizeof(address_parser_data_set_t));
    data_set->f = fopen(filename, "r");
    if (data_set->f == NULL) {
        free(data_set);
        return NULL;
    }

    data_set->tokens = token_array_new();
    data_set->tokenized_str = NULL;
    data_set->labels = cstring_array_new();
    data_set->separators = uint32_array_new();
    data_set->language = char_array_new_size(MAX_LANGUAGE_LEN);
    data_set->country = char_array_new_size(MAX_COUNTRY_CODE_LEN);

    return data_set;
}


bool address_parser_data_set_tokenize_line(address_parser_data_set_t *data_set, char *input) {
    token_array *tokens = data_set->tokens;
    uint32_array *separators = data_set->separators;
    cstring_array *labels = data_set->labels;

    size_t count = 0;

    token_t token;

    uint32_t i = 0;
    char *str = NULL;

    cstring_array *pairs = cstring_array_split(input, " ", 1, &count);
    size_t num_pairs = cstring_array_num_strings(pairs);

    char *label = NULL;

    // First populate token array
    cstring_array_foreach(pairs, i, str, {
        size_t pair_len = strlen(str);

        char *last_separator = strrchr(str, (int)'/');

        if (last_separator == NULL) {
            log_error("All tokens must be delimited with '/'\n");
            return false;
        }

        uint32_t last_separator_index = last_separator - str;

        label = str + last_separator_index + 1;

        if (strcmp(label, FIELD_SEPARATOR_LABEL) == 0) {
            uint32_array_pop(separators);
            uint32_array_push(separators, ADDRESS_SEPARATOR_FIELD | ADDRESS_SEPARATOR_FIELD_INTERNAL);
            continue;
        } else if (strcmp(label, SEPARATOR_LABEL) == 0) {
            uint32_array_pop(separators);
            uint32_array_push(separators, ADDRESS_SEPARATOR_FIELD_INTERNAL);
            continue;
        }

        token.offset = pairs->indices->a[i];
        token.len = last_separator_index;

        scanner_t scanner = scanner_from_string(input + token.offset, token.len);
        token.type = scan_token(&scanner);
        if (ADDRESS_PARSER_IS_SEPARATOR(token.type)) {
            uint32_array_push(separators, ADDRESS_SEPARATOR_FIELD_INTERNAL);
            continue;
        } else if (ADDRESS_PARSER_IS_IGNORABLE(token.type)) {
            // shouldn't happen but just in case
            continue;
        } else {
            uint32_array_push(separators, ADDRESS_SEPARATOR_NONE);
        }

        cstring_array_add_string(labels, label);

        token_array_push(tokens, token);
    })

    cstring_array_destroy(pairs);

    return true;
}



bool address_parser_data_set_next(address_parser_data_set_t *data_set) {
    if (data_set == NULL) return false;

    char *line = file_getline(data_set->f);
    if (line == NULL) {
        return false;
    }

    size_t token_count;

    cstring_array *fields = cstring_array_split(line, TAB_SEPARATOR, TAB_SEPARATOR_LEN, &token_count);

    free(line);

    if (token_count != ADDRESS_PARSER_FILE_NUM_TOKENS) {
        log_error("Token count did not match, ected %d, got %zu\n", ADDRESS_PARSER_FILE_NUM_TOKENS, token_count);
    }

    char *language = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_LANGUAGE);
    char *country = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_COUNTRY);
    char *address = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_ADDRESS);

    log_debug("Doing: %s\n", address);

    char *normalized = address_parser_normalize_string(address);
    bool is_normalized = normalized != NULL;
    if (!is_normalized) {
        log_debug("could not normalize\n");
        normalized = strdup(address);
    }

    log_debug("Normalized: %s\n", normalized);

    token_array *tokens = data_set->tokens;
    cstring_array *labels = data_set->labels;
    uint32_array *separators = data_set->separators;

    token_array_clear(tokens);
    cstring_array_clear(labels);
    uint32_array_clear(separators);
    size_t len = strlen(normalized);

    char_array_clear(data_set->country);
    char_array_add(data_set->country, country);

    char_array_clear(data_set->language);
    char_array_add(data_set->language, language);

    tokenized_string_t *tokenized_str = NULL;

    if (address_parser_data_set_tokenize_line(data_set, normalized)) {
        // Add tokens as discrete strings for easier use in feature functions
        bool copy_tokens = true;
        tokenized_str = tokenized_string_from_tokens(normalized, data_set->tokens, copy_tokens);        
    }

    data_set->tokenized_str = tokenized_str;

    free(normalized);
    cstring_array_destroy(fields);

    return tokenized_str != NULL;
}


void address_parser_data_set_destroy(address_parser_data_set_t *self) {
    if (self == NULL) return;

    if (self->f != NULL) {
        fclose(self->f);
    }

    if (self->tokens != NULL) {
        token_array_destroy(self->tokens);
    }

    if (self->labels != NULL) {
        cstring_array_destroy(self->labels);
    }

    if (self->separators != NULL) {
        uint32_array_destroy(self->separators);
    }

    if (self->language != NULL) {
        char_array_destroy(self->language);
    }

    if (self->country != NULL) {
        char_array_destroy(self->country);
    }

    free(self);
}
