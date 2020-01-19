#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "libpostal_types.h"
#include "address_dictionary.h"
#include "transliterate.h"
#include "numex.h"

#ifdef _WIN32
#ifdef LIBPOSTAL_EXPORTS
#define LIBPOSTAL_EXPORT __declspec(dllexport)
#else
#define LIBPOSTAL_EXPORT __declspec(dllimport)
#endif
#elif __GNUC__ >= 4
#define LIBPOSTAL_EXPORT __attribute__ ((visibility("default")))
#else
#define LIBPOSTAL_EXPORT
#endif

typedef struct libpostal {
    transliteration_table_t *trans_table;
    numex_table_t *numex_table;
    address_dictionary_t *address_dict;
} libpostal_t;

LIBPOSTAL_EXPORT libpostal_normalize_options_t libpostal_get_default_options(void);

LIBPOSTAL_EXPORT char **libpostal_expand_address(libpostal_t *instance, char *input, libpostal_normalize_options_t options, size_t *n);
LIBPOSTAL_EXPORT char **libpostal_expand_address_root(libpostal_t *instance, char *input, libpostal_normalize_options_t options, size_t *n);

LIBPOSTAL_EXPORT void libpostal_expansion_array_destroy(char **expansions, size_t n);

/*
Address parser
*/

typedef struct libpostal_address_parser_response {
    size_t num_components;
    char **components;
    char **labels;
} libpostal_address_parser_response_t;

typedef libpostal_address_parser_response_t libpostal_parsed_address_components_t;

typedef struct libpostal_address_parser_options {
    char *language;
    char *country;
} libpostal_address_parser_options_t;

LIBPOSTAL_EXPORT void libpostal_address_parser_response_destroy(libpostal_address_parser_response_t *self);

LIBPOSTAL_EXPORT libpostal_address_parser_options_t libpostal_get_address_parser_default_options(void);

LIBPOSTAL_EXPORT libpostal_address_parser_response_t *libpostal_parse_address(libpostal_t *instance, char *address, libpostal_address_parser_options_t options);

LIBPOSTAL_EXPORT bool libpostal_parser_print_features(bool print_features);

/*
Language classification
*/

typedef struct libpostal_language_classifier_response {
    size_t num_languages;
    char **languages;
    double *probs;
} libpostal_language_classifier_response_t;

LIBPOSTAL_EXPORT libpostal_language_classifier_response_t *libpostal_classify_language(libpostal_t *instance, char *address);

LIBPOSTAL_EXPORT void libpostal_language_classifier_response_destroy(libpostal_language_classifier_response_t *self);

/*
Deduping
*/


// Near-dupe hashing methods

LIBPOSTAL_EXPORT libpostal_near_dupe_hash_options_t libpostal_get_near_dupe_hash_default_options(void);
LIBPOSTAL_EXPORT char **libpostal_near_dupe_hashes(libpostal_t *instance, size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options, size_t *num_hashes);
LIBPOSTAL_EXPORT char **libpostal_near_dupe_hashes_languages(libpostal_t *instance, size_t num_components, char **labels, char **values, libpostal_near_dupe_hash_options_t options, size_t num_languages, char **languages, size_t *num_hashes);

// Dupe language classification

LIBPOSTAL_EXPORT char **libpostal_place_languages(libpostal_t *instance, size_t num_components, char **labels, char **values, size_t *num_languages);

// Pairwise dupe methods

typedef enum {
    LIBPOSTAL_NULL_DUPLICATE_STATUS = -1,
    LIBPOSTAL_NON_DUPLICATE = 0,
    LIBPOSTAL_POSSIBLE_DUPLICATE_NEEDS_REVIEW = 3,
    LIBPOSTAL_LIKELY_DUPLICATE = 6,
    LIBPOSTAL_EXACT_DUPLICATE = 9,
} libpostal_duplicate_status_t;

typedef struct libpostal_duplicate_options {
    size_t num_languages;
    char **languages;
} libpostal_duplicate_options_t;


LIBPOSTAL_EXPORT libpostal_duplicate_options_t libpostal_get_default_duplicate_options(void);
LIBPOSTAL_EXPORT libpostal_duplicate_options_t libpostal_get_duplicate_options_with_languages(size_t num_languages, char **languages);

LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_name_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_street_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_house_number_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_po_box_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_unit_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_floor_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_postal_code_duplicate(libpostal_t *instance, char *value1, char *value2, libpostal_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_duplicate_status_t libpostal_is_toponym_duplicate(libpostal_t *instance, size_t num_components1, char **labels1, char **values1, size_t num_components2, char **labels2, char **values2, libpostal_duplicate_options_t options);

// Pairwise fuzzy dupe methods, return status & similarity

typedef struct libpostal_fuzzy_duplicate_options {
    size_t num_languages;
    char **languages;
    double needs_review_threshold;
    double likely_dupe_threshold;
} libpostal_fuzzy_duplicate_options_t;

typedef struct libpostal_fuzzy_duplicate_status {
    libpostal_duplicate_status_t status;
    double similarity;
} libpostal_fuzzy_duplicate_status_t;

LIBPOSTAL_EXPORT libpostal_fuzzy_duplicate_options_t libpostal_get_default_fuzzy_duplicate_options(void);
LIBPOSTAL_EXPORT libpostal_fuzzy_duplicate_options_t libpostal_get_default_fuzzy_duplicate_options_with_languages(size_t num_languages, char **languages);

LIBPOSTAL_EXPORT libpostal_fuzzy_duplicate_status_t libpostal_is_name_duplicate_fuzzy(libpostal_t *instance, size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options);
LIBPOSTAL_EXPORT libpostal_fuzzy_duplicate_status_t libpostal_is_street_duplicate_fuzzy(libpostal_t *instance, size_t num_tokens1, char **tokens1, double *token_scores1, size_t num_tokens2, char **tokens2, double *token_scores2, libpostal_fuzzy_duplicate_options_t options);

// Setup/teardown methods

LIBPOSTAL_EXPORT libpostal_t *libpostal_setup(void);
LIBPOSTAL_EXPORT libpostal_t *libpostal_setup_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown(libpostal_t **instance);

LIBPOSTAL_EXPORT bool libpostal_setup_parser(void);
LIBPOSTAL_EXPORT bool libpostal_setup_parser_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_parser(void);

LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier(void);
LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_language_classifier(void);

/* Tokenization and token normalization APIs */

LIBPOSTAL_EXPORT libpostal_token_t *libpostal_tokenize(char *input, bool whitespace, size_t *n);

LIBPOSTAL_EXPORT char *libpostal_normalize_string_languages(libpostal_t *instance, char *input, uint64_t options, size_t num_languages, char **languages);
LIBPOSTAL_EXPORT char *libpostal_normalize_string(libpostal_t *instance, char *input, uint64_t options);


typedef struct libpostal_normalized_token {
    char *str;
    libpostal_token_t token;
} libpostal_normalized_token_t;

LIBPOSTAL_EXPORT libpostal_normalized_token_t *libpostal_normalized_tokens(libpostal_t *instance, char *input, uint64_t string_options, uint64_t token_options, bool whitespace, size_t *n);
LIBPOSTAL_EXPORT libpostal_normalized_token_t *libpostal_normalized_tokens_languages(libpostal_t *instance, char *input, uint64_t string_options, uint64_t token_options, bool whitespace, size_t num_languages, char **languages, size_t *n);


#ifdef __cplusplus
}
#endif

#endif
