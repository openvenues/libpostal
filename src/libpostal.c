#include <stdlib.h>

#include "libpostal.h"

#include "klib/khash.h"
#include "klib/ksort.h"
#include "log/log.h"

#include "address_dictionary.h"
#include "address_parser.h"
#include "expand.h"

#include "language_classifier.h"
#include "normalize.h"
#include "scanner.h"
#include "string_utils.h"
#include "token_types.h"

static libpostal_normalize_options_t LIBPOSTAL_DEFAULT_OPTIONS = {
        .languages = NULL,
        .num_languages = 0,
        .address_components = LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_PO_BOX | LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ENTRANCE | LIBPOSTAL_ADDRESS_STAIRCASE | LIBPOSTAL_ADDRESS_POSTAL_CODE,
        .latin_ascii = true,
        .transliterate = true,
        .strip_accents = true,
        .decompose = true,
        .lowercase = true,
        .trim_string = true,
        .drop_parentheticals = true,
        .replace_numeric_hyphens = false,
        .delete_numeric_hyphens = false,
        .split_alpha_from_numeric = true,
        .replace_word_hyphens = true,
        .delete_word_hyphens = true,
        .delete_final_periods = true,
        .delete_acronym_periods = true,
        .drop_english_possessives = true,
        .delete_apostrophes = true,
        .expand_numex = true,
        .roman_numerals = true
};

libpostal_normalize_options_t libpostal_get_default_options(void) {
    return LIBPOSTAL_DEFAULT_OPTIONS;
}

char **libpostal_expand_address(char *input, libpostal_normalize_options_t options, size_t *n) {
    return expand_address(input, options, n);
}

void libpostal_expansion_array_destroy(char **expansions, size_t n) {
    expansion_array_destroy(expansions, n);
}

void libpostal_address_parser_response_destroy(libpostal_address_parser_response_t *self) {
    if (self == NULL) return;

    for (size_t i = 0; i < self->num_components; i++) {
        if (self->components != NULL) {
            free(self->components[i]);
        }

        if (self->labels != NULL) {
            free(self->labels[i]);
        }
    }

    if (self->components != NULL) {
        free(self->components);
    }

    if (self->labels != NULL) {
        free(self->labels);
    }

    free(self);
}

static libpostal_address_parser_options_t LIBPOSTAL_ADDRESS_PARSER_DEFAULT_OPTIONS =  {
    .language = NULL,
    .country = NULL
};

inline libpostal_address_parser_options_t libpostal_get_address_parser_default_options(void) {
    return LIBPOSTAL_ADDRESS_PARSER_DEFAULT_OPTIONS;
}

libpostal_address_parser_response_t *libpostal_parse_address(char *address, libpostal_address_parser_options_t options) {
    libpostal_address_parser_response_t *parsed = address_parser_parse(address, options.language, options.country);

    if (parsed == NULL) {
        log_error("Parser returned NULL\n");
        return NULL;
    }

    return parsed;
}

bool libpostal_parser_print_features(bool print_features) {
    return address_parser_print_features(print_features);
}

bool libpostal_setup_datadir(char *datadir) {
    char *transliteration_path = NULL;
    char *numex_path = NULL;
    char *address_dictionary_path = NULL;

    if (datadir != NULL) {
        transliteration_path = path_join(3, datadir, LIBPOSTAL_TRANSLITERATION_SUBDIR, TRANSLITERATION_DATA_FILE);
        numex_path = path_join(3, datadir, LIBPOSTAL_NUMEX_SUBDIR, NUMEX_DATA_FILE);
        address_dictionary_path = path_join(3, datadir, LIBPOSTAL_ADDRESS_EXPANSIONS_SUBDIR, ADDRESS_DICTIONARY_DATA_FILE);
    }

    if (!transliteration_module_setup(transliteration_path)) {
        log_error("Error loading transliteration module, dir=%s\n", transliteration_path);
        return false;
    }

    if (!numex_module_setup(numex_path)) {
        log_error("Error loading numex module, dir=%s\n", numex_path);
        return false;
    }

    if (!address_dictionary_module_setup(address_dictionary_path)) {
        log_error("Error loading dictionary module, dir=%s\n", address_dictionary_path);
        return false;
    }

    if (transliteration_path != NULL) {
        free(transliteration_path);
    }

    if (numex_path != NULL) {
        free(numex_path);
    }

    if (address_dictionary_path != NULL) {
        free(address_dictionary_path);
    }

    return true;
}

bool libpostal_setup(void) {
    return libpostal_setup_datadir(NULL);
}

bool libpostal_setup_language_classifier_datadir(char *datadir) {
    char *language_classifier_dir = NULL;

    if (datadir != NULL) {
        language_classifier_dir = path_join(2, datadir, LIBPOSTAL_LANGUAGE_CLASSIFIER_SUBDIR);
    }

    if (!language_classifier_module_setup(language_classifier_dir)) {
        log_error("Error loading language classifier, dir=%s\n", language_classifier_dir);
        return false;
    }

    if (language_classifier_dir != NULL) {
        free(language_classifier_dir);
    }

    return true;
}


libpostal_token_t *libpostal_tokenize(char *input, bool whitespace, size_t *n) {
    token_array *tokens = NULL;
    if (!whitespace) {
        tokens = tokenize(input);
    } else {
        tokens = tokenize_keep_whitespace(input);
    }

    if (tokens == NULL) {
        return NULL;
    }

    libpostal_token_t *a = tokens->a;
    *n = tokens->n;
    free(tokens);
    return a;
}

char *libpostal_normalize_string(char *str, uint64_t options) {
    if (options & LIBPOSTAL_NORMALIZE_STRING_LATIN_ASCII) {
        return normalize_string_latin(str, strlen(str), options);
    } else {
        return normalize_string_utf8(str, options);
    }
}

libpostal_normalized_token_t *libpostal_normalized_tokens(char *input, uint64_t string_options, uint64_t token_options, bool whitespace, size_t *n) {
    if (input == NULL) {
        return NULL;
    }
    char *normalized = libpostal_normalize_string(input, string_options);
    if (normalized == NULL) {
        return NULL;
    }

    token_array *tokens = NULL;
    if (!whitespace) {
        tokens = tokenize(normalized);
    } else {
        tokens = tokenize_keep_whitespace(normalized);
    }

    if (tokens == NULL || tokens->a == NULL) {
        free(normalized);
        return NULL;
    }

    size_t num_tokens = tokens->n;
    token_t *token_array = tokens->a;
    char_array *normalized_token = char_array_new_size(strlen(normalized));

    libpostal_normalized_token_t *result = malloc(sizeof(libpostal_normalized_token_t) * num_tokens);

    for (size_t i = 0; i < num_tokens; i++) {
        token_t token = token_array[i];
        char_array_clear(normalized_token);
        add_normalized_token(normalized_token, normalized, token, token_options);
        char *token_str = strdup(char_array_get_string(normalized_token));
        result[i] = (libpostal_normalized_token_t){token_str, token};
    }

    free(normalized);
    token_array_destroy(tokens);
    char_array_destroy(normalized_token);

    *n = num_tokens;
    return result;
}

bool libpostal_setup_language_classifier(void) {
    return libpostal_setup_language_classifier_datadir(NULL);
}

bool libpostal_setup_parser_datadir(char *datadir) {
    char *parser_dir = NULL;

    if (datadir != NULL) {
        parser_dir = path_join(2, datadir, LIBPOSTAL_ADDRESS_PARSER_SUBDIR);
    }

    if (!address_parser_module_setup(parser_dir)) {
        log_error("Error loading address parser module, dir=%s\n", parser_dir);
        return false;
    }

    if (parser_dir != NULL) {
        free(parser_dir);
    }

    return true;
}

bool libpostal_setup_parser(void) {
    return libpostal_setup_parser_datadir(NULL);
}

void libpostal_teardown(void) {
    transliteration_module_teardown();

    numex_module_teardown();

    address_dictionary_module_teardown();
}

void libpostal_teardown_language_classifier(void) {
    language_classifier_module_teardown();
}

void libpostal_teardown_parser(void) {
    address_parser_module_teardown();
}
