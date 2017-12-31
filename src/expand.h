#ifndef EXPAND_H
#define EXPAND_H

#include <stdlib.h>
#include <stdio.h>

#include "libpostal.h"

#include "address_dictionary.h"
#include "collections.h"
#include "klib/khash.h"
#include "klib/ksort.h"
#include "trie_search.h"

typedef struct phrase_language {
    char *language;
    phrase_t phrase;
} phrase_language_t;

VECTOR_INIT(phrase_language_array, phrase_language_t)

#define ks_lt_phrase_language(a, b) ((a).phrase.start < (b).phrase.start || ((a).phrase.start == (b).phrase.start && (a).phrase.len > (b).phrase.len))

KSORT_INIT(phrase_language_array, phrase_language_t, ks_lt_phrase_language)

uint64_t get_normalize_token_options(libpostal_normalize_options_t options);
uint64_t get_normalize_string_options(libpostal_normalize_options_t options);

void add_normalized_strings_token(cstring_array *strings, char *str, token_t token, libpostal_normalize_options_t options);
void add_postprocessed_string(cstring_array *strings, char *str, libpostal_normalize_options_t options);

address_expansion_array *valid_affix_expansions(phrase_t phrase, libpostal_normalize_options_t options);

void cat_affix_expansion(char_array *key, char *str, address_expansion_t expansion, token_t token, phrase_t phrase, libpostal_normalize_options_t options);
bool add_affix_expansions(string_tree_t *tree, char *str, char *lang, token_t token, phrase_t prefix, phrase_t suffix, libpostal_normalize_options_t options, bool with_period);

bool expand_affixes(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options);
bool expand_affixes_period(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options);
bool add_period_affixes_or_token(string_tree_t *tree, char *str, token_t token, libpostal_normalize_options_t options);

bool normalize_ordinal_suffixes(string_tree_t *tree, char *str, char *lang, token_t token, size_t i, token_t prev_token, libpostal_normalize_options_t options);

void add_normalized_strings_tokenized(string_tree_t *tree, char *str, token_array *tokens, libpostal_normalize_options_t options);


bool address_phrase_is_ignorable_for_components(phrase_t phrase, uint32_t address_components);
bool address_phrase_is_edge_ignorable_for_components(phrase_t phrase, uint32_t address_components);
bool address_phrase_is_possible_root_for_components(phrase_t phrase, uint32_t address_components);
bool address_phrase_is_specifier_for_components(phrase_t phrase, uint32_t address_components);
bool address_phrase_is_valid_for_components(phrase_t phrase, uint32_t address_components);


typedef enum {
    EXPAND_PHRASES,
    KEEP_PHRASES,
    DELETE_PHRASES
} expansion_phrase_option_t;

cstring_array *expand_address(char *input, libpostal_normalize_options_t options, size_t *n);
cstring_array *expand_address_phrase_option(char *input, libpostal_normalize_options_t options, size_t *n, expansion_phrase_option_t phrase_option);
cstring_array *expand_address_root(char *input, libpostal_normalize_options_t options, size_t *n);
void expansion_array_destroy(char **expansions, size_t n);

#endif
