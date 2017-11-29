#ifndef LIBPOSTAL_H
#define LIBPOSTAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

#define LIBPOSTAL_MAX_LANGUAGE_LEN 4

// Doing these as #defines so we can duplicate the values exactly in Python


typedef enum {
    LIBPOSTAL_TOKEN_TYPE_END = 0,                   // Null byte
    // Word types
    LIBPOSTAL_TOKEN_TYPE_WORD = 1,                  // Any letter-only word (includes all unicode letters)
    LIBPOSTAL_TOKEN_TYPE_ABBREVIATION = 2,          // Loose abbreviations (roughly anything containing a "." as we don't care about sentences in addresses)
    LIBPOSTAL_TOKEN_TYPE_IDEOGRAPHIC_CHAR = 3,      // For languages that don't separate on whitespace (e.g. Chinese, Japanese, Korean), separate by character
    LIBPOSTAL_TOKEN_TYPE_HANGUL_SYLLABLE = 4,       // Hangul syllable sequences which contain more than one codepoint
    LIBPOSTAL_TOKEN_TYPE_ACRONYM = 5,               // Specifically things like U.N. where we may delete internal periods

    LIBPOSTAL_TOKEN_TYPE_PHRASE = 10,               // Not part of the first stage tokenizer, but may be used after phrase parsing

    // Special tokens
    LIBPOSTAL_TOKEN_TYPE_EMAIL = 20,                // Make sure emails are tokenized altogether
    LIBPOSTAL_TOKEN_TYPE_URL = 21,                  // Make sure urls are tokenized altogether
    LIBPOSTAL_TOKEN_TYPE_US_PHONE = 22,             // US phone number (with or without country code)
    LIBPOSTAL_TOKEN_TYPE_INTL_PHONE = 23,           // A non-US phone number (must have country code)

    // Numbers and numeric types
    LIBPOSTAL_TOKEN_TYPE_NUMERIC = 50,              // Any sequence containing a digit
    LIBPOSTAL_TOKEN_TYPE_ORDINAL = 51,              // 1st, 2nd, 1er, 1 etc.
    LIBPOSTAL_TOKEN_TYPE_ROMAN_NUMERAL = 52,        // II, III, VI, etc.
    LIBPOSTAL_TOKEN_TYPE_IDEOGRAPHIC_NUMBER = 53,   // All numeric ideographic characters, includes e.g. Han numbers and chars like "²"

    // Punctuation types, may separate a phrase
    LIBPOSTAL_TOKEN_TYPE_PERIOD = 100,
    LIBPOSTAL_TOKEN_TYPE_EXCLAMATION = 101,
    LIBPOSTAL_TOKEN_TYPE_QUESTION_MARK = 102,
    LIBPOSTAL_TOKEN_TYPE_COMMA = 103,
    LIBPOSTAL_TOKEN_TYPE_COLON = 104,
    LIBPOSTAL_TOKEN_TYPE_SEMICOLON = 105,
    LIBPOSTAL_TOKEN_TYPE_PLUS = 106,
    LIBPOSTAL_TOKEN_TYPE_AMPERSAND = 107,
    LIBPOSTAL_TOKEN_TYPE_AT_SIGN = 108,
    LIBPOSTAL_TOKEN_TYPE_POUND = 109,
    LIBPOSTAL_TOKEN_TYPE_ELLIPSIS = 110,
    LIBPOSTAL_TOKEN_TYPE_DASH = 111,
    LIBPOSTAL_TOKEN_TYPE_BREAKING_DASH = 112,
    LIBPOSTAL_TOKEN_TYPE_HYPHEN = 113,
    LIBPOSTAL_TOKEN_TYPE_PUNCT_OPEN = 114,
    LIBPOSTAL_TOKEN_TYPE_PUNCT_CLOSE = 115,
    LIBPOSTAL_TOKEN_TYPE_DOUBLE_QUOTE = 119,
    LIBPOSTAL_TOKEN_TYPE_SINGLE_QUOTE = 120,
    LIBPOSTAL_TOKEN_TYPE_OPEN_QUOTE = 121,
    LIBPOSTAL_TOKEN_TYPE_CLOSE_QUOTE = 122,
    LIBPOSTAL_TOKEN_TYPE_SLASH = 124,
    LIBPOSTAL_TOKEN_TYPE_BACKSLASH = 125,
    LIBPOSTAL_TOKEN_TYPE_GREATER_THAN = 126,
    LIBPOSTAL_TOKEN_TYPE_LESS_THAN = 127,

    // Non-letters and whitespace
    LIBPOSTAL_TOKEN_TYPE_OTHER = 200,
    LIBPOSTAL_TOKEN_TYPE_WHITESPACE = 300,
    LIBPOSTAL_TOKEN_TYPE_NEWLINE = 301,

    LIBPOSTAL_TOKEN_TYPE_INVALID_CHAR = 500
} libpostal_token_type_t;


/* 
Address dictionaries
*/
// Bit set, should be able to keep it at a short (uint16_t)
#define LIBPOSTAL_ADDRESS_NONE 0
#define LIBPOSTAL_ADDRESS_ANY (1 << 0)
#define LIBPOSTAL_ADDRESS_NAME (1 << 1)
#define LIBPOSTAL_ADDRESS_HOUSE_NUMBER (1 << 2)
#define LIBPOSTAL_ADDRESS_STREET (1 << 3)
#define LIBPOSTAL_ADDRESS_UNIT (1 << 4)
#define LIBPOSTAL_ADDRESS_LEVEL (1 << 5)
#define LIBPOSTAL_ADDRESS_STAIRCASE (1 << 6)
#define LIBPOSTAL_ADDRESS_ENTRANCE (1 << 7)

#define LIBPOSTAL_ADDRESS_CATEGORY (1 << 8)
#define LIBPOSTAL_ADDRESS_NEAR (1 << 9)

#define LIBPOSTAL_ADDRESS_TOPONYM (1 << 13)
#define LIBPOSTAL_ADDRESS_POSTAL_CODE (1 << 14)
#define LIBPOSTAL_ADDRESS_PO_BOX (1 << 15)
#define LIBPOSTAL_ADDRESS_ALL ((1 << 16) - 1)

typedef struct libpostal_normalize_options {
    // List of language codes
    char **languages;  
    size_t num_languages;
    uint16_t address_components;

    // String options
    bool latin_ascii;
    bool transliterate;
    bool strip_accents;
    bool decompose;
    bool lowercase;
    bool trim_string;
    bool drop_parentheticals;
    bool replace_numeric_hyphens;
    bool delete_numeric_hyphens;
    bool split_alpha_from_numeric;
    bool replace_word_hyphens;
    bool delete_word_hyphens;
    bool delete_final_periods;
    bool delete_acronym_periods;
    bool drop_english_possessives;
    bool delete_apostrophes;
    bool expand_numex;
    bool roman_numerals;

} libpostal_normalize_options_t;

LIBPOSTAL_EXPORT libpostal_normalize_options_t libpostal_get_default_options(void);

LIBPOSTAL_EXPORT char **libpostal_expand_address(char *input, libpostal_normalize_options_t options, size_t *n);

LIBPOSTAL_EXPORT void libpostal_expansion_array_destroy(char **expansions, size_t n);

/*
Address parser
*/

typedef struct libpostal_address_parser_response {
    size_t num_components;
    char **components;
    char **labels;
} libpostal_address_parser_response_t;

typedef struct libpostal_address_parser_options {
    char *language;
    char *country;
} libpostal_address_parser_options_t;

LIBPOSTAL_EXPORT void libpostal_address_parser_response_destroy(libpostal_address_parser_response_t *self);

LIBPOSTAL_EXPORT libpostal_address_parser_options_t libpostal_get_address_parser_default_options(void);

LIBPOSTAL_EXPORT libpostal_address_parser_response_t *libpostal_parse_address(char *address, libpostal_address_parser_options_t options);

LIBPOSTAL_EXPORT bool libpostal_parser_print_features(bool print_features);

// Setup/teardown methods

LIBPOSTAL_EXPORT bool libpostal_setup(void);
LIBPOSTAL_EXPORT bool libpostal_setup_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown(void);

LIBPOSTAL_EXPORT bool libpostal_setup_parser(void);
LIBPOSTAL_EXPORT bool libpostal_setup_parser_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_parser(void);

LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier(void);
LIBPOSTAL_EXPORT bool libpostal_setup_language_classifier_datadir(char *datadir);
LIBPOSTAL_EXPORT void libpostal_teardown_language_classifier(void);

/* Tokenization and token normalization APIs */

typedef struct libpostal_token {
    size_t offset;
    size_t len;
    uint16_t type;
} libpostal_token_t;

LIBPOSTAL_EXPORT libpostal_token_t *libpostal_tokenize(char *input, bool whitespace, size_t *n);

// Normalize string options
#define LIBPOSTAL_NORMALIZE_STRING_LATIN_ASCII 1 << 0
#define LIBPOSTAL_NORMALIZE_STRING_TRANSLITERATE 1 << 1
#define LIBPOSTAL_NORMALIZE_STRING_STRIP_ACCENTS 1 << 2
#define LIBPOSTAL_NORMALIZE_STRING_DECOMPOSE 1 << 3
#define LIBPOSTAL_NORMALIZE_STRING_LOWERCASE 1 << 4
#define LIBPOSTAL_NORMALIZE_STRING_TRIM 1 << 5
#define LIBPOSTAL_NORMALIZE_STRING_REPLACE_HYPHENS 1 << 6
#define LIBPOSTAL_NORMALIZE_STRING_COMPOSE 1 << 7
#define LIBPOSTAL_NORMALIZE_STRING_SIMPLE_LATIN_ASCII 1 << 8
#define LIBPOSTAL_NORMALIZE_STRING_REPLACE_NUMEX 1 << 9

// Normalize token options
#define LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_HYPHENS 1 << 0
#define LIBPOSTAL_NORMALIZE_TOKEN_DELETE_HYPHENS 1 << 1
#define LIBPOSTAL_NORMALIZE_TOKEN_DELETE_FINAL_PERIOD 1 << 2
#define LIBPOSTAL_NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS 1 << 3
#define LIBPOSTAL_NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES 1 << 4
#define LIBPOSTAL_NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE 1 << 5
#define LIBPOSTAL_NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC 1 << 6
#define LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_DIGITS 1 << 7
#define LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_NUMERIC_TOKEN_LETTERS 1 << 8
#define LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS 1 << 9

#define LIBPOSTAL_NORMALIZE_DEFAULT_STRING_OPTIONS (LIBPOSTAL_NORMALIZE_STRING_LATIN_ASCII | LIBPOSTAL_NORMALIZE_STRING_COMPOSE | LIBPOSTAL_NORMALIZE_STRING_TRIM | LIBPOSTAL_NORMALIZE_STRING_REPLACE_HYPHENS | LIBPOSTAL_NORMALIZE_STRING_STRIP_ACCENTS | LIBPOSTAL_NORMALIZE_STRING_LOWERCASE)

#define LIBPOSTAL_NORMALIZE_DEFAULT_TOKEN_OPTIONS (LIBPOSTAL_NORMALIZE_TOKEN_REPLACE_HYPHENS | LIBPOSTAL_NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | LIBPOSTAL_NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | LIBPOSTAL_NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES | LIBPOSTAL_NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE)

#define LIBPOSTAL_NORMALIZE_TOKEN_OPTIONS_DROP_PERIODS (LIBPOSTAL_NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | LIBPOSTAL_NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS)

#define LIBPOSTAL_NORMALIZE_DEFAULT_TOKEN_OPTIONS_NUMERIC (LIBPOSTAL_NORMALIZE_DEFAULT_TOKEN_OPTIONS | LIBPOSTAL_NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC)

LIBPOSTAL_EXPORT char *libpostal_normalize_string(char *input, uint64_t options);


typedef struct libpostal_normalized_token {
    char *str;
    libpostal_token_t token;
} libpostal_normalized_token_t;

libpostal_normalized_token_t *libpostal_normalized_tokens(char *input, uint64_t string_options, uint64_t token_options, bool whitespace, size_t *n);

#ifdef __cplusplus
}
#endif

#endif
