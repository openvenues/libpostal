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
    data_set->normalizations = cstring_array_new();
    data_set->norm = 0;
    data_set->labels = cstring_array_new();
    data_set->separators = uint32_array_new();
    data_set->language = char_array_new_size(MAX_LANGUAGE_LEN);
    data_set->country = char_array_new_size(MAX_COUNTRY_CODE_LEN);

    return data_set;
}

bool address_parser_data_set_rewind(address_parser_data_set_t *self) {
    if (self == NULL || self->f == NULL) return false;

    return (fseek(self->f, 0, SEEK_SET) == 0);
}


bool address_parser_all_normalizations(cstring_array *strings, char *str, char *language) {
    if (strings == NULL) return false;

    char *lowercased = normalize_string_utf8(str, ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS);
    if (lowercased == NULL) {
        return false;
    }

    cstring_array_add_string(strings, lowercased);

    char *latin_normalized = normalize_string_latin(str, strlen(str), ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS_LATIN);
    if (latin_normalized != NULL) {
        if (!string_equals(latin_normalized, lowercased)) {
            cstring_array_add_string(strings, latin_normalized);
        }
        free(latin_normalized);
    }

    char *trans_name = NULL;
    char *transliterated = NULL;
    char *transliterated_utf8_normalized = NULL;

    foreach_transliterator(SCRIPT_LATIN, language, trans_name, {
        if (!string_equals(trans_name, LATIN_ASCII)) {
            transliterated = transliterate(trans_name, str, strlen(str));
            if (transliterated != NULL) {
                transliterated_utf8_normalized = normalize_string_utf8(transliterated, ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS_UTF8);
                if (transliterated_utf8_normalized != NULL) {
                    if (!string_equals(transliterated_utf8_normalized, lowercased)) {
                        cstring_array_add_string(strings, transliterated_utf8_normalized);
                    }
                    free(transliterated_utf8_normalized);
                    transliterated_utf8_normalized = NULL;
                } else {
                    cstring_array_add_string(strings, transliterated);
                }

                free(transliterated);
                transliterated = NULL;
            }
        }
    })

    char *utf8_normalized = normalize_string_utf8(str, ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS_UTF8);
    if (utf8_normalized != NULL) {
        if (!string_equals(utf8_normalized, lowercased)) {
            cstring_array_add_string(strings, utf8_normalized);
        }
        free(utf8_normalized);
    }

    free(lowercased);

    return true;
}

bool address_parser_data_set_tokenize_line(address_parser_data_set_t *self, char *input) {
    token_array *tokens = self->tokens;
    uint32_array *separators = self->separators;
    cstring_array *labels = self->labels;

    size_t count = 0;

    token_t token;

    uint32_t i = 0;
    char *str = NULL;

    cstring_array *pairs = cstring_array_split_ignore_consecutive(input, " ", 1, &count);
    size_t num_pairs = cstring_array_num_strings(pairs);

    char *label = NULL;

    // First populate token array
    cstring_array_foreach(pairs, i, str, {
        size_t pair_len = strlen(str);

        char *last_separator = strrchr(str, (int)'/');

        if (last_separator == NULL) {
            log_error("All tokens must be delimited with '/'\n");
            log_error("line = %s\n", input);
            log_error("str = %s, i=%d\n", str, i);
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
        size_t expected_len = last_separator_index;

        scanner_t scanner = scanner_from_string(input + token.offset, expected_len);
        token.type = scan_token(&scanner);
        token.len = scanner.cursor - scanner.start;

        if (token.len == expected_len) {
            if (ADDRESS_PARSER_IS_SEPARATOR(token.type)) {
                uint32_array_pop(separators);
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
        } else {
            /* If normalizing the string turned one token into several e.g. ½ => 1/2
               add all the tokens where offset = (token.offset + sub_token.offset)
               with the same label as the parent.
            */
            token_array *sub_tokens = token_array_new();
            if (sub_tokens == NULL) {
                log_error("Error allocating sub-token array\n");
                return false;
            }
            tokenize_add_tokens(sub_tokens, input + token.offset, expected_len, false);
            for (size_t j = 0; j < sub_tokens->n; j++) {
                token_t sub_token = sub_tokens->a[j];
                // Add the offset of the parent "token"
                sub_token.offset = token.offset + sub_token.offset;

                if (ADDRESS_PARSER_IS_SEPARATOR(sub_token.type)) {
                    uint32_array_push(separators, ADDRESS_SEPARATOR_FIELD_INTERNAL);
                    continue;
                } else if (ADDRESS_PARSER_IS_IGNORABLE(sub_token.type)) {
                    continue;
                } else {
                    uint32_array_push(separators, ADDRESS_SEPARATOR_NONE);
                }

                cstring_array_add_string(labels, label);
                token_array_push(tokens, sub_token);
            }

            token_array_destroy(sub_tokens);

        }

    })

    cstring_array_destroy(pairs);

    return true;
}



bool address_parser_data_set_next(address_parser_data_set_t *self) {
    if (self == NULL) return false;

    cstring_array *fields = NULL;

    if (self->norm == 0 || self->norm >= cstring_array_num_strings(self->normalizations)) {
        char *line = file_getline(self->f);
        if (line == NULL) {
            return false;
        }

        size_t token_count;

        fields = cstring_array_split(line, TAB_SEPARATOR, TAB_SEPARATOR_LEN, &token_count);

        free(line);

        if (token_count != ADDRESS_PARSER_FILE_NUM_TOKENS) {
            log_error("Token count did not match, expected %d, got %zu\n", ADDRESS_PARSER_FILE_NUM_TOKENS, token_count);
            return false;
        }

        char *language = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_LANGUAGE);
        char *country = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_COUNTRY);
        char *address = cstring_array_get_string(fields, ADDRESS_PARSER_FIELD_ADDRESS);

        char_array_clear(self->country);
        char_array_add(self->country, country);

        char_array_clear(self->language);
        char_array_add(self->language, language);

        log_debug("Doing: %s\n", address);

        cstring_array_clear(self->normalizations);

        if (!address_parser_all_normalizations(self->normalizations, address, language) || cstring_array_num_strings(self->normalizations) == 0) {
            log_error("Error during string normalization\n");
            return false;
        }
        self->norm = 0;
    }

    char *normalized = cstring_array_get_string(self->normalizations, self->norm);

    token_array *tokens = self->tokens;
    cstring_array *labels = self->labels;
    uint32_array *separators = self->separators;

    token_array_clear(tokens);
    cstring_array_clear(labels);
    uint32_array_clear(separators);
    size_t len = strlen(normalized);

    tokenized_string_t *tokenized_str = NULL;

    if (address_parser_data_set_tokenize_line(self, normalized)) {
        // Add tokens as discrete strings for easier use in feature functions
        bool copy_tokens = true;
        tokenized_str = tokenized_string_from_tokens(normalized, self->tokens, copy_tokens);        
    }

    self->tokenized_str = tokenized_str;

    self->norm++;

    if (fields != NULL) {
        cstring_array_destroy(fields);
    }

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

    if (self->normalizations != NULL) {
        cstring_array_destroy(self->normalizations);
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
