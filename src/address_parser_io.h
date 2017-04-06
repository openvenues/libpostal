#ifndef ADDRESS_PARSER_IO_H
#define ADDRESS_PARSER_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "address_parser.h"
#include "collections.h"
#include "file_utils.h"
#include "scanner.h"
#include "string_utils.h"

#define AMBIGUOUS_LANGUAGE "xxx"
#define UNKNOWN_LANGUAGE "unk"

enum address_parser_training_data_fields {
    ADDRESS_PARSER_FIELD_LANGUAGE,
    ADDRESS_PARSER_FIELD_COUNTRY,
    ADDRESS_PARSER_FIELD_ADDRESS,
    ADDRESS_PARSER_FILE_NUM_TOKENS
};

typedef struct address_parser_data_set {
    FILE *f;
    token_array *tokens;
    tokenized_string_t *tokenized_str;
    cstring_array *normalizations;
    size_t norm;
    cstring_array *labels;
    uint32_array *separators;
    char_array *language;
    char_array *country;
} address_parser_data_set_t;


address_parser_data_set_t *address_parser_data_set_init(char *filename);
bool address_parser_data_set_rewind(address_parser_data_set_t *self);
bool address_parser_data_set_tokenize_line(address_parser_data_set_t *self, char *input);
bool address_parser_data_set_next(address_parser_data_set_t *self);
void address_parser_data_set_destroy(address_parser_data_set_t *self);

#endif