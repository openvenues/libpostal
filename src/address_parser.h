/*
address_parser.h
----------------

International address parser, designed to use OSM training data,
over 40M addresses formatted with the OpenCage address formatting
templates: https://github.com/OpenCageData/address-formatting.

This is a sequence modeling problem similar to e.g. part-of-speech
tagging, named entity recognition, etc. in which we have a sequence
of inputs (words/tokens) and want to predict a sequence of outputs
(labeled part-of-address tags). This is a supervised learning model
and the training data is created in the Python geodata package
included with this repo. Example record:

en  us  123/house_number Fake/road Street/road Brooklyn/city NY/state 12345/postcode

Where the fields are: {language, country, tagged address}.

After training, the address parser can take as input a tokenized
input string e.g. "123 Fake Street Brooklyn NY 12345" and parse
it into:

{
    "house_number": "123",
    "road": "Fake Street",
    "city": "Brooklyn",
    "state": "NY",
    "postcode": "12345"
}

The model used is a greedy averaged perceptron rather than something
like a CRF since there's ample training data from OSM and the accuracy
on this task is already very high with the simpler model.

However, it is still worth investigating CRFs as they are relatively fast
at prediction time for a small number of tags, can often achieve better
performance and are robust to correlated features, which may not be true
with the general error-driven averaged perceptron.

*/
#ifndef ADDRESS_PARSER_H
#define ADDRESS_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "libpostal.h"
#include "libpostal_config.h"

#include "averaged_perceptron.h"
#include "averaged_perceptron_tagger.h"
#include "collections.h"
#include "crf.h"
#include "graph.h"
#include "normalize.h"
#include "string_utils.h"

#define DEFAULT_ADDRESS_PARSER_PATH LIBPOSTAL_ADDRESS_PARSER_DIR PATH_SEPARATOR "address_parser.dat"

#define ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS NORMALIZE_STRING_COMPOSE | NORMALIZE_STRING_LOWERCASE | NORMALIZE_STRING_SIMPLE_LATIN_ASCII
#define ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS_LATIN NORMALIZE_STRING_COMPOSE | NORMALIZE_STRING_LOWERCASE | NORMALIZE_STRING_LATIN_ASCII
#define ADDRESS_PARSER_NORMALIZE_STRING_OPTIONS_UTF8 NORMALIZE_STRING_COMPOSE | NORMALIZE_STRING_LOWERCASE | NORMALIZE_STRING_STRIP_ACCENTS

#define ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | NORMALIZE_TOKEN_REPLACE_DIGITS
#define ADDRESS_PARSER_NORMALIZE_ADMIN_TOKEN_OPTIONS ADDRESS_PARSER_NORMALIZE_TOKEN_OPTIONS ^ NORMALIZE_TOKEN_REPLACE_DIGITS
#define ADDRESS_PARSER_NORMALIZE_POSTAL_CODE_TOKEN_OPTIONS ADDRESS_PARSER_NORMALIZE_ADMIN_TOKEN_OPTIONS | NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC

#define ADDRESS_SEPARATOR_NONE 0
#define ADDRESS_SEPARATOR_FIELD_INTERNAL 1 << 0
#define ADDRESS_SEPARATOR_FIELD 1 << 1

#define ADDRESS_PARSER_IS_SEPARATOR(token_type) ((token_type) == COMMA || (token_type) == NEWLINE || (token_type) == HYPHEN || (token_type) == DASH || (token_type) == BREAKING_DASH|| (token_type) == SEMICOLON || (token_type) == PUNCT_OPEN || (token_type) == PUNCT_CLOSE )
#define ADDRESS_PARSER_IS_IGNORABLE(token_type) ((token.type) == INVALID_CHAR || (token.type) == PERIOD || (token_type) == COLON )

#define SEPARATOR_LABEL "sep"
#define FIELD_SEPARATOR_LABEL "fsep"

#define ADDRESS_COMPONENT_NON_BOUNDARY 0
#define ADDRESS_COMPONENT_SUBURB 1 << 3
#define ADDRESS_COMPONENT_CITY_DISTRICT 1 << 4
#define ADDRESS_COMPONENT_CITY 1 << 5
#define ADDRESS_COMPONENT_ISLAND 1 << 7
#define ADDRESS_COMPONENT_STATE_DISTRICT 1 << 8
#define ADDRESS_COMPONENT_STATE 1 << 9
#define ADDRESS_COMPONENT_COUNTRY_REGION 1 << 11
#define ADDRESS_COMPONENT_COUNTRY 1 << 13
#define ADDRESS_COMPONENT_WORLD_REGION 1 << 14

typedef enum {
    ADDRESS_PARSER_BOUNDARY_NONE,
    ADDRESS_PARSER_BOUNDARY_SUBURB,
    ADDRESS_PARSER_BOUNDARY_CITY_DISTRICT,
    ADDRESS_PARSER_BOUNDARY_CITY,
    ADDRESS_PARSER_BOUNDARY_STATE_DISTRICT,
    ADDRESS_PARSER_BOUNDARY_ISLAND,
    ADDRESS_PARSER_BOUNDARY_STATE,
    ADDRESS_PARSER_BOUNDARY_COUNTRY_REGION,
    ADDRESS_PARSER_BOUNDARY_COUNTRY,
    ADDRESS_PARSER_BOUNDARY_WORLD_REGION,
    NUM_ADDRESS_PARSER_BOUNDARY_TYPES
} address_parser_boundary_components;


#define ADDRESS_PARSER_LABEL_HOUSE "house"
#define ADDRESS_PARSER_LABEL_HOUSE_NUMBER "house_number"
#define ADDRESS_PARSER_LABEL_PO_BOX "po_box"
#define ADDRESS_PARSER_LABEL_BUILDING "building"
#define ADDRESS_PARSER_LABEL_ENTRANCE "entrance"
#define ADDRESS_PARSER_LABEL_STAIRCASE "staircase"
#define ADDRESS_PARSER_LABEL_LEVEL "level"
#define ADDRESS_PARSER_LABEL_UNIT "unit"
#define ADDRESS_PARSER_LABEL_ROAD "road"
#define ADDRESS_PARSER_LABEL_METRO_STATION "metro_station"
#define ADDRESS_PARSER_LABEL_SUBURB "suburb"
#define ADDRESS_PARSER_LABEL_CITY_DISTRICT "city_district"
#define ADDRESS_PARSER_LABEL_CITY "city"
#define ADDRESS_PARSER_LABEL_STATE_DISTRICT  "state_district"
#define ADDRESS_PARSER_LABEL_ISLAND "island"
#define ADDRESS_PARSER_LABEL_STATE  "state"
#define ADDRESS_PARSER_LABEL_POSTAL_CODE  "postcode"
#define ADDRESS_PARSER_LABEL_COUNTRY_REGION  "country_region"
#define ADDRESS_PARSER_LABEL_COUNTRY  "country"
#define ADDRESS_PARSER_LABEL_WORLD_REGION "world_region"

#define ADDRESS_PARSER_LABEL_WEBSITE "website"
#define ADDRESS_PARSER_LABEL_TELEPHONE "phone"

typedef union address_parser_types {
    uint32_t value;
    struct {
        uint32_t components:16;     // Bitset of components
        uint32_t most_common:16;    // Most common component as short integer enum value 
    };
} address_parser_types_t;

VECTOR_INIT(address_parser_types_array, address_parser_types_t)

typedef struct address_parser_context {
    char *language;
    char *country;
    cstring_array *features;
    cstring_array *prev_tag_features;
    cstring_array *prev2_tag_features;
    // Temporary strings used at each token during feature extraction
    char_array *phrase;
    char_array *context_phrase;
    char_array *long_context_phrase;
    char_array *prefix_phrase;
    char_array *context_prefix_phrase;
    char_array *long_context_prefix_phrase;
    char_array *suffix_phrase;
    char_array *context_suffix_phrase;
    char_array *long_context_suffix_phrase;
    char_array *component_phrase;
    char_array *context_component_phrase;
    char_array *long_context_component_phrase;
    // ngrams and prefix/suffix features
    cstring_array *ngrams;
    // For hyphenated words
    char_array *sub_token;
    token_array *sub_tokens;
    // Strings/arrays relating to the sentence
    uint32_array *separators;
    cstring_array *normalized;
    token_array *normalized_tokens;
    cstring_array *normalized_admin;
    token_array *normalized_admin_tokens;
    // Known phrases
    phrase_array *address_dictionary_phrases;
    int64_array *address_phrase_memberships; // Index in address_dictionary_phrases or -1
    phrase_array *component_phrases;
    int64_array *component_phrase_memberships; // Index in component_phrases or -1
    phrase_array *postal_code_phrases;
    int64_array *postal_code_phrase_memberships; // Index in postal_code_phrases or -1
    phrase_array *prefix_phrases;
    phrase_array *suffix_phrases;
    // The tokenized string used to conveniently access both words as C strings and tokens by index
    tokenized_string_t *tokenized_str;
} address_parser_context_t;

typedef union postal_code_context_value {
    uint64_t value;
    struct {
        uint64_t postcode:32;
        uint64_t admin:32;
    };
} postal_code_context_value_t;

#define POSTAL_CODE_CONTEXT(pc, ad) ((postal_code_context_value_t){.postcode = (pc), .admin = (ad) })

typedef enum address_parser_model_type {
    ADDRESS_PARSER_TYPE_GREEDY_AVERAGED_PERCEPTRON,
    ADDRESS_PARSER_TYPE_CRF
} address_parser_model_type_t;

typedef struct parser_options {
    uint64_t rare_word_threshold;
    bool print_features;
} parser_options_t;

// Can add other gazetteers as well
typedef struct address_parser {
    parser_options_t options;
    size_t num_classes;
    address_parser_model_type_t model_type;
    union {
        averaged_perceptron_t *ap;
        crf_t *crf;
    } model;
    address_parser_context_t *context;
    trie_t *vocab;
    trie_t *phrases;
    address_parser_types_array *phrase_types;
    trie_t *postal_codes;
    graph_t *postal_code_contexts;
} address_parser_t;

// General usage

address_parser_t *address_parser_new(void);
address_parser_t *address_parser_new_options(parser_options_t options);
address_parser_t *get_address_parser(void);
bool address_parser_load(char *dir);

bool address_parser_print_features(bool print_features);
libpostal_address_parser_response_t *address_parser_parse(char *address, char *language, char *country);
void address_parser_destroy(address_parser_t *self);

char *address_parser_normalize_string(char *str);
void address_parser_normalize_token(cstring_array *array, char *str, token_t token);

bool address_parser_predict(address_parser_t *self, address_parser_context_t *context, cstring_array *token_labels, tagger_feature_function feature_function, tokenized_string_t *tokenized_str);

address_parser_context_t *address_parser_context_new(void);
void address_parser_context_destroy(address_parser_context_t *self);

void address_parser_context_fill(address_parser_context_t *context, address_parser_t *parser, tokenized_string_t *tokenized_str, char *language, char *country);

// Feature function
bool address_parser_features(void *self, void *ctx, tokenized_string_t *str, uint32_t i);

// I/O methods

bool address_parser_load(char *dir);
bool address_parser_save(address_parser_t *self, char *output_dir);

// Module setup/teardown

bool address_parser_module_setup(char *dir);
void address_parser_module_teardown(void);


#endif
