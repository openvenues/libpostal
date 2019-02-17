#include <stdlib.h>

#include "expand.h"

#include "log/log.h"

#include "address_dictionary.h"
#include "collections.h"
#include "constants.h"
#include "language_classifier.h"
#include "numex.h"
#include "normalize.h"
#include "scanner.h"
#include "string_utils.h"
#include "token_types.h"
#include "transliterate.h"


#define DEFAULT_KEY_LEN 32

#define EXCESSIVE_PERMUTATIONS 100

inline uint64_t get_normalize_token_options(libpostal_normalize_options_t options) {
    uint64_t normalize_token_options = 0;

    normalize_token_options |= options.delete_final_periods ? NORMALIZE_TOKEN_DELETE_FINAL_PERIOD : 0;
    normalize_token_options |= options.delete_acronym_periods ? NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS : 0;
    normalize_token_options |= options.drop_english_possessives ? NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES : 0;
    normalize_token_options |= options.delete_apostrophes ? NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE : 0;

    return normalize_token_options;
}

inline uint64_t get_normalize_string_options(libpostal_normalize_options_t options) {
    uint64_t normalize_string_options = 0;
    normalize_string_options |= options.transliterate ? NORMALIZE_STRING_TRANSLITERATE : 0;
    normalize_string_options |= options.latin_ascii ? NORMALIZE_STRING_LATIN_ASCII : 0;
    normalize_string_options |= options.decompose ? NORMALIZE_STRING_DECOMPOSE : 0;
    normalize_string_options |= options.strip_accents ? NORMALIZE_STRING_STRIP_ACCENTS : 0;
    normalize_string_options |= options.lowercase ? NORMALIZE_STRING_LOWERCASE : 0;
    normalize_string_options |= options.trim_string ? NORMALIZE_STRING_TRIM : 0;
    normalize_string_options |= options.expand_numex ? NORMALIZE_STRING_REPLACE_NUMEX : 0;

    return normalize_string_options;
}

void add_normalized_strings_token(cstring_array *strings, char *str, token_t token, libpostal_normalize_options_t options) {

    uint64_t normalize_token_options = get_normalize_token_options(options);

    if (token.type != WHITESPACE ) {

        bool contains_hyphen = string_contains_hyphen_len(str + token.offset, token.len);

        if (!contains_hyphen || token.type == HYPHEN) {
            log_debug("str = %s, token = {%zu, %zu, %u}\n", str, token.offset, token.len, token.type);
            normalize_token(strings, str, token, normalize_token_options);
        } else if (is_word_token(token.type)) {

            size_t prefix_hyphen_len = string_hyphen_prefix_len(str + token.offset, token.len);
            if (prefix_hyphen_len > 0) {
                token.offset += prefix_hyphen_len;
            }

            size_t suffix_hyphen_len = string_hyphen_suffix_len(str + token.offset, token.len);
            if (suffix_hyphen_len > 0) {
                token.len -= suffix_hyphen_len;
            }

            normalize_token(strings, str, token, normalize_token_options);

            if (options.replace_word_hyphens) {
                normalize_token_options |= NORMALIZE_TOKEN_REPLACE_HYPHENS;
                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_REPLACE_HYPHENS;
            }

            if (options.delete_word_hyphens) {
                normalize_token_options |= NORMALIZE_TOKEN_DELETE_HYPHENS;
                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_DELETE_HYPHENS;
            }

        } else if (is_numeric_token(token.type)) {

            normalize_token(strings, str, token, normalize_token_options);

            if (options.replace_word_hyphens || options.replace_numeric_hyphens) {
                if (options.replace_word_hyphens) {
                    normalize_token_options |= NORMALIZE_TOKEN_REPLACE_HYPHENS;
                }

                if (options.replace_numeric_hyphens) {
                    normalize_token_options |= NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS;
                }

                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_REPLACE_HYPHENS | NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS;
            }

            if (options.delete_numeric_hyphens) {
                normalize_token_options |= NORMALIZE_TOKEN_DELETE_HYPHENS;
                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_DELETE_HYPHENS;
            }
        }

        if (is_numeric_token(token.type) && options.split_alpha_from_numeric) {
            bool split_alpha_from_numeric = true;

            for (size_t i = 0; i < options.num_languages; i++) {
                char *lang = options.languages[i];
                if (valid_ordinal_suffix_len(str, token, NULL_TOKEN, lang) > 1) {
                    split_alpha_from_numeric = false;
                    break;
                }
            }

            if (split_alpha_from_numeric) {
                normalize_token_options |= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
            }
        }
    } else {
        cstring_array_add_string(strings, " ");
    }
}

void add_postprocessed_string(cstring_array *strings, char *str, libpostal_normalize_options_t options) {
    cstring_array_add_string(strings, str);

    if (options.roman_numerals) {
        char *numex_replaced = replace_numeric_expressions(str, LATIN_LANGUAGE_CODE);
        if (numex_replaced != NULL) {
            cstring_array_add_string(strings, numex_replaced);
            free(numex_replaced);
        }

    }

}

address_expansion_array *valid_affix_expansions(phrase_t phrase, libpostal_normalize_options_t options) {
    uint32_t expansion_index = phrase.data;
    address_expansion_value_t *value = address_dictionary_get_expansions(expansion_index);
    if (value != NULL && value->components & options.address_components) {
        return value->expansions;
    }

    return NULL;
}

inline void cat_affix_expansion(char_array *key, char *str, address_expansion_t expansion, token_t token, phrase_t phrase, libpostal_normalize_options_t options) {
    if (expansion.canonical_index != NULL_CANONICAL_INDEX) {
        char *canonical = address_dictionary_get_canonical(expansion.canonical_index);
        uint64_t normalize_string_options = get_normalize_string_options(options);
        char *canonical_normalized = normalize_string_latin(canonical, strlen(canonical), normalize_string_options);
        canonical = canonical_normalized != NULL ? canonical_normalized : canonical;

        char_array_cat(key, canonical);
        if (canonical_normalized != NULL) {
            free(canonical_normalized);
        }
    } else {
        char_array_cat_len(key, str + token.offset + phrase.start, phrase.len);
    }
}


bool add_affix_expansions(string_tree_t *tree, char *str, char *lang, token_t token, phrase_t prefix, phrase_t suffix, libpostal_normalize_options_t options, bool with_period) {
    cstring_array *strings = tree->strings;

    size_t skip_period = with_period ? 1 : 0;

    bool have_suffix = suffix.len > 0 && suffix.len < token.len;
    bool have_prefix = prefix.len > 0 && prefix.len + with_period < token.len;

    if (!have_suffix && !have_prefix) {
        return false;
    }

    address_expansion_array *prefix_expansions = NULL;
    address_expansion_array *suffix_expansions = NULL;

    address_expansion_t prefix_expansion;
    address_expansion_t suffix_expansion;

    char *expansion;

    size_t num_strings = 0;
    char *root_word = NULL;
    size_t root_len;
    token_t root_token;
    cstring_array *root_strings = NULL;
    int add_space = 0;
    int spaces = 0;

    size_t prefix_start, prefix_end, root_end, suffix_start;

    if (have_prefix) {
        prefix_expansions = valid_affix_expansions(prefix, options);
        if (prefix_expansions == NULL) have_prefix = false;
    }

    if (have_suffix) {
        suffix_expansions = valid_affix_expansions(suffix, options);
        if (suffix_expansions == NULL) have_suffix = false;
    }

    if (!have_suffix && !have_prefix) {
        return false;
    }

    char_array *key = char_array_new_size(token.len);

    if (have_prefix && have_suffix) {
        for (size_t i = 0; i < prefix_expansions->n; i++) {
            prefix_expansion = prefix_expansions->a[i];
            char_array_clear(key);

            cat_affix_expansion(key, str, prefix_expansion, token, prefix, options);
            prefix_start = key->n - 1;

            add_space = (int)prefix_expansion.separable || with_period;
            if (prefix.len + skip_period + suffix.len < token.len && !prefix_expansion.separable) {
                add_space = suffix_expansion.separable || with_period;
            }

            for (spaces = skip_period; spaces <= add_space; spaces++) {
                key->n = prefix_start;
                if (spaces) {
                    char_array_cat(key, " ");
                }

                prefix_end = key->n;

                if (prefix.len + skip_period + suffix.len < token.len) {
                    root_len = token.len - suffix.len - prefix.len - skip_period;
                    size_t root_start = token.offset + prefix.len + skip_period;
                    size_t prefix_hyphen_len = string_hyphen_prefix_len(str + root_start, root_len);
                    root_start += prefix_hyphen_len;
                    root_len -= prefix_hyphen_len;
                    size_t suffix_hyphen_len = string_hyphen_suffix_len(str + root_start, root_len);
                    root_len -= suffix_hyphen_len;
                    root_token = (token_t){root_start, root_len, token.type};
                    root_strings = cstring_array_new_size(root_len);
                    add_normalized_strings_token(root_strings, str, root_token, options);
                    num_strings = cstring_array_num_strings(root_strings);

                    for (size_t j = 0; j < num_strings; j++) {
                        key->n = prefix_end;
                        root_word = cstring_array_get_string(root_strings, j);
                        char_array_cat(key, root_word);
                        root_end = key->n - 1;

                        for (size_t k = 0; k < suffix_expansions->n; k++) {
                            key->n = root_end;
                            suffix_expansion = suffix_expansions->a[k];

                            int add_suffix_space = suffix_expansion.separable;

                            suffix_start = key->n;
                            for (int suffix_spaces = skip_period; suffix_spaces <= add_suffix_space; suffix_spaces++) {
                                key->n = suffix_start;
                                if (suffix_spaces) {
                                    char_array_cat(key, " ");
                                }

                                cat_affix_expansion(key, str, suffix_expansion, token, suffix, options);

                                expansion = char_array_get_string(key);
                                cstring_array_add_string(strings, expansion);

                            }


                        }
                    }

                    cstring_array_destroy(root_strings);
                    root_strings = NULL;

                } else {
                    for (size_t j = 0; j < suffix_expansions->n; j++) {
                        key->n = prefix_end - skip_period;
                        suffix_expansion = suffix_expansions->a[j];

                        cat_affix_expansion(key, str, suffix_expansion, token, suffix, options);

                        expansion = char_array_get_string(key);
                        cstring_array_add_string(tree->strings, expansion);
                    }
                }
            }

        }
    } else if (have_suffix) {
        log_debug("suffix.start=%" PRId32 "\n", suffix.start);
        root_len = suffix.start;
        root_token = (token_t){token.offset, root_len, token.type};
        log_debug("root_len=%zu\n", root_len);
        log_debug("root_token = {%zu, %zu, %u}\n", root_token.offset, root_token.len, root_token.type);

        root_strings = cstring_array_new_size(root_len + 1);
        add_normalized_strings_token(root_strings, str, root_token, options);
        num_strings = cstring_array_num_strings(root_strings);

        log_debug("num_strings = %zu\n", num_strings);

        for (size_t j = 0; j < num_strings; j++) {
            char_array_clear(key);
            root_word = cstring_array_get_string(root_strings, j);
            log_debug("root_word=%s\n", root_word);
            char_array_cat(key, root_word);
            root_end = key->n - 1;

            for (size_t k = 0; k < suffix_expansions->n; k++) {
                key->n = root_end;
                suffix_expansion = suffix_expansions->a[k];

                add_space = (suffix_expansion.separable || with_period) && suffix.len < token.len;
                suffix_start = key->n;

                for (int spaces = skip_period; spaces <= add_space; spaces++) {
                    key->n = suffix_start;
                    if (spaces) {
                        char_array_cat(key, " ");
                    }

                    cat_affix_expansion(key, str, suffix_expansion, token, suffix, options);

                    expansion = char_array_get_string(key);
                    cstring_array_add_string(tree->strings, expansion);
                }
            }
        }
    } else if (have_prefix) {
        if (prefix.len + skip_period <= token.len) {
            root_len = token.len - prefix.len - skip_period;
            size_t root_start = token.offset + prefix.len + skip_period;
            size_t prefix_hyphen_len = string_hyphen_prefix_len(str + root_start, root_len);
            root_start += prefix_hyphen_len;
            root_len -= prefix_hyphen_len;
            size_t suffix_hyphen_len = string_hyphen_suffix_len(str + root_start, root_len);
            root_len -= suffix_hyphen_len;
            root_token = (token_t){root_start, root_len, token.type};
            root_strings = cstring_array_new_size(root_len);
            add_normalized_strings_token(root_strings, str, root_token, options);
            num_strings = cstring_array_num_strings(root_strings);

        } else {
            root_strings = cstring_array_new_size(token.len);
            add_normalized_strings_token(root_strings, str, token, options);
            num_strings = cstring_array_num_strings(root_strings);

            for (size_t k = 0; k < num_strings; k++) {
                root_word = cstring_array_get_string(root_strings, k);
                cstring_array_add_string(tree->strings, root_word);
            }

            char_array_destroy(key);
            cstring_array_destroy(root_strings);
            return false;

        }

        for (size_t j = 0; j < prefix_expansions->n; j++) {
            char_array_clear(key);
            prefix_expansion = prefix_expansions->a[j];

            cat_affix_expansion(key, str, prefix_expansion, token, prefix, options);
            prefix_end = key->n - 1;

            add_space = (prefix_expansion.separable || with_period) && prefix.len + skip_period < token.len;
            for (int spaces = skip_period; spaces <= add_space; spaces++) {
                key->n = prefix_end;
                if (spaces) {
                    char_array_cat(key, " ");
                }
                size_t prefix_space_len = key->n - spaces;
                for (size_t k = 0; k < num_strings; k++) {
                    key->n = prefix_space_len;
                    root_word = cstring_array_get_string(root_strings, k);
                    char_array_cat(key, root_word);

                    expansion = char_array_get_string(key);
                    cstring_array_add_string(tree->strings, expansion);
                }

            }
        }
    }

    char_array_destroy(key);

    if (root_strings != NULL) {
        cstring_array_destroy(root_strings);
    }

    return true;

}

inline bool expand_affixes(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options) {
    phrase_t suffix = search_address_dictionaries_suffix(str + token.offset, token.len, lang);

    phrase_t prefix = search_address_dictionaries_prefix(str + token.offset, token.len, lang);

    if ((suffix.len == 0 && prefix.len == 0)) return false;

    bool with_period = false;

    return add_affix_expansions(tree, str, lang, token, prefix, suffix, options, with_period);
}

inline bool expand_affixes_period(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options) {
    ssize_t first_period_index = string_next_period_len(str + token.offset, token.len);
    if (first_period_index > 0) {
        ssize_t next_period_index = string_next_period_len(str + token.offset + first_period_index + 1, token.len - first_period_index - 1);
        // Token contains only one period or one + a final period                    
        if (next_period_index < 0 || next_period_index == token.len - 1) {
            phrase_t prefix = search_address_dictionaries_substring(str + token.offset, first_period_index, lang);

            phrase_t suffix = search_address_dictionaries_substring(str + token.offset + first_period_index + 1, token.len - first_period_index - 1, lang);
            if (suffix.len > 0) {
                suffix.start = first_period_index + 1;
            }

            if (suffix.len == 0 && prefix.len == 0) return false;

            bool with_period = true;

            return add_affix_expansions(tree, str, lang, token, prefix, suffix, options, with_period);
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool add_period_affixes_or_token(string_tree_t *tree, char *str, token_t token, libpostal_normalize_options_t options) {
    bool have_period_affixes = false;
    if (string_contains_period_len(str + token.offset, token.len)) {
        for (size_t l = 0; l < options.num_languages; l++) {
            char *lang = options.languages[l];
            if (expand_affixes_period(tree, str, lang, token, options)) {
                have_period_affixes = true;
                break;
            }
        }
    }

    if (!have_period_affixes) {
        string_tree_add_string_len(tree, str + token.offset, token.len);
    }

    return have_period_affixes;
}


static inline uint32_t gazetteer_ignorable_components(uint16_t dictionary_id) {
    switch (dictionary_id) {
        case DICTIONARY_ACADEMIC_DEGREE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_BUILDING_TYPE:
            return LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_UNIT;
        case DICTIONARY_COMPANY_TYPE:
            return LIBPOSTAL_ADDRESS_NAME;
        case DICTIONARY_DIRECTIONAL:
            return LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_ELISION:
            return LIBPOSTAL_ADDRESS_ANY;
        case DICTIONARY_ENTRANCE:
            return LIBPOSTAL_ADDRESS_ENTRANCE;
        case DICTIONARY_HOUSE_NUMBER:
            return LIBPOSTAL_ADDRESS_HOUSE_NUMBER;
        case DICTIONARY_LEVEL_NUMBERED:
            return LIBPOSTAL_ADDRESS_LEVEL;
        case DICTIONARY_LEVEL_STANDALONE:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_LEVEL_MEZZANINE:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_LEVEL| LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_LEVEL_BASEMENT:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_LEVEL_SUB_BASEMENT:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_NUMBER:
            return LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_PO_BOX | LIBPOSTAL_ADDRESS_STAIRCASE | LIBPOSTAL_ADDRESS_ENTRANCE | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_NO_NUMBER:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_PERSONAL_TITLE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_PLACE_NAME:
            return LIBPOSTAL_ADDRESS_NAME;
        case DICTIONARY_POST_OFFICE:
            return LIBPOSTAL_ADDRESS_PO_BOX;
        case DICTIONARY_POSTAL_CODE:
            return LIBPOSTAL_ADDRESS_POSTAL_CODE;
        case DICTIONARY_QUALIFIER:
            return LIBPOSTAL_ADDRESS_TOPONYM;
        case DICTIONARY_STAIRCASE:
            return LIBPOSTAL_ADDRESS_STAIRCASE;
        case DICTIONARY_STOPWORD:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_CATEGORY | LIBPOSTAL_ADDRESS_NEAR | LIBPOSTAL_ADDRESS_TOPONYM;
        case DICTIONARY_STREET_TYPE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_UNIT_NUMBERED:
            return LIBPOSTAL_ADDRESS_UNIT;
        case DICTIONARY_UNIT_STANDALONE:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_ANY);
        case DICTIONARY_UNIT_DIRECTION:
            return LIBPOSTAL_ADDRESS_ALL ^ (LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_ANY);
        default:
            return LIBPOSTAL_ADDRESS_NONE;
    }
}


static inline uint32_t gazetteer_valid_components(uint16_t dictionary_id) {
    switch (dictionary_id) {
        case DICTIONARY_DIRECTIONAL:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_CATEGORY | LIBPOSTAL_ADDRESS_NEAR | LIBPOSTAL_ADDRESS_TOPONYM | LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_STAIRCASE | LIBPOSTAL_ADDRESS_ENTRANCE;
        case DICTIONARY_STOPWORD:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_CATEGORY | LIBPOSTAL_ADDRESS_NEAR | LIBPOSTAL_ADDRESS_TOPONYM;
        case DICTIONARY_STREET_NAME:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_STREET_TYPE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_SYNONYM:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET | LIBPOSTAL_ADDRESS_CATEGORY | LIBPOSTAL_ADDRESS_NEAR | LIBPOSTAL_ADDRESS_TOPONYM;
        default:
            return LIBPOSTAL_ADDRESS_NONE;
    }
}

static inline uint32_t gazetteer_edge_ignorable_components(uint16_t dictionary_id) {
    switch (dictionary_id) {
        // Pre/post directionals can be removed if there are non-phrase tokens
        case DICTIONARY_DIRECTIONAL:
            return LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_COMPANY_TYPE:
            return LIBPOSTAL_ADDRESS_NAME;
        case DICTIONARY_PLACE_NAME:
            return LIBPOSTAL_ADDRESS_NAME;
        default:
            return LIBPOSTAL_ADDRESS_NONE;
    }
}

static inline uint32_t gazetteer_specifier_components(uint16_t dictionary_id) {
    switch (dictionary_id) {
        case DICTIONARY_LEVEL_STANDALONE:
            return LIBPOSTAL_ADDRESS_LEVEL;
        case DICTIONARY_LEVEL_MEZZANINE:
            return LIBPOSTAL_ADDRESS_LEVEL;
        case DICTIONARY_LEVEL_BASEMENT:
            return LIBPOSTAL_ADDRESS_LEVEL;
        case DICTIONARY_LEVEL_SUB_BASEMENT:
            return LIBPOSTAL_ADDRESS_LEVEL;
        case DICTIONARY_UNIT_STANDALONE:
            return LIBPOSTAL_ADDRESS_UNIT;
        default:
            return LIBPOSTAL_ADDRESS_NONE;
    }
}


static inline uint32_t gazetteer_possible_root_components(uint16_t dictionary_id) {
    switch (dictionary_id) {
        case DICTIONARY_ACADEMIC_DEGREE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_DIRECTIONAL:
            return LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_PERSONAL_TITLE:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_NUMBER:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_PLACE_NAME:
            return LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_QUALIFIER:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_STREET_NAME:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_SYNONYM:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        case DICTIONARY_TOPONYM:
            return LIBPOSTAL_ADDRESS_NAME | LIBPOSTAL_ADDRESS_STREET;
        default:
            return LIBPOSTAL_ADDRESS_NONE;
    }
}

static const uint16_t NUMERIC_ADDRESS_COMPONENTS = (LIBPOSTAL_ADDRESS_HOUSE_NUMBER | LIBPOSTAL_ADDRESS_UNIT | LIBPOSTAL_ADDRESS_LEVEL | LIBPOSTAL_ADDRESS_PO_BOX | LIBPOSTAL_ADDRESS_POSTAL_CODE | LIBPOSTAL_ADDRESS_STAIRCASE | LIBPOSTAL_ADDRESS_ENTRANCE | LIBPOSTAL_ADDRESS_STREET);

typedef enum {
    GAZETTEER_MATCH_IGNORABLE,
    GAZETTEER_MATCH_EDGE_IGNORABLE,
    GAZETTEER_MATCH_POSSIBLE_ROOT,
    GAZETTEER_MATCH_SPECIFIER,
    GAZETTEER_MATCH_VALID_COMPONENTS
} gazetteer_match_type_t;


static inline bool address_expansion_matches_type_for_components(address_expansion_t expansion, uint32_t address_components, gazetteer_match_type_t match_type) {
    for (uint32_t j = 0; j < expansion.num_dictionaries; j++) {
        uint16_t dictionary_id = expansion.dictionary_ids[j];
        uint32_t components = 0;
        switch (match_type) {
            case GAZETTEER_MATCH_IGNORABLE:
                components =  gazetteer_ignorable_components(dictionary_id);
                break;
            case GAZETTEER_MATCH_EDGE_IGNORABLE:
                components =  gazetteer_edge_ignorable_components(dictionary_id);
                break;
            case GAZETTEER_MATCH_POSSIBLE_ROOT:
                components =  gazetteer_possible_root_components(dictionary_id);
                break;
            case GAZETTEER_MATCH_SPECIFIER:
                components =  gazetteer_specifier_components(dictionary_id);
                break;
            case GAZETTEER_MATCH_VALID_COMPONENTS:
                components = gazetteer_valid_components(dictionary_id);
                break;
            default:
                break;
        }
        if (components & address_components) {
            return true;
        }
    }
    return false;
}

bool address_expansion_is_ignorable_for_components(address_expansion_t expansion, uint32_t address_components) {
    return address_expansion_matches_type_for_components(expansion, address_components, GAZETTEER_MATCH_IGNORABLE);
}

bool address_expansion_is_edge_ignorable_for_components(address_expansion_t expansion, uint32_t address_components) {
    return address_expansion_matches_type_for_components(expansion, address_components, GAZETTEER_MATCH_EDGE_IGNORABLE);
}

bool address_expansion_is_possible_root_for_components(address_expansion_t expansion, uint32_t address_components) {
    return address_expansion_matches_type_for_components(expansion, address_components, GAZETTEER_MATCH_POSSIBLE_ROOT);
}

bool address_expansion_is_specifier_for_components(address_expansion_t expansion, uint32_t address_components) {
    return address_expansion_matches_type_for_components(expansion, address_components, GAZETTEER_MATCH_SPECIFIER);
}

bool address_expansion_is_valid_for_components(address_expansion_t expansion, uint32_t address_components) {
    return address_expansion_matches_type_for_components(expansion, address_components, GAZETTEER_MATCH_VALID_COMPONENTS);
}


bool address_phrase_matches_type_for_components(phrase_t phrase, uint32_t address_components, gazetteer_match_type_t match_type) {
    uint32_t expansion_index = phrase.data;
    address_expansion_value_t *value = address_dictionary_get_expansions(expansion_index);

    if (value == NULL) return false;

    address_expansion_array *expansions = value->expansions;
    if (expansions == NULL) return false;

    for (size_t i = 0; i < expansions->n; i++) {
        address_expansion_t expansion = expansions->a[i];

        if (address_expansion_matches_type_for_components(expansion, address_components, match_type)) {
            return true;
        }
    }
    return false;
}

inline bool address_phrase_is_ignorable_for_components(phrase_t phrase, uint32_t address_components) {
    return address_phrase_matches_type_for_components(phrase, address_components, GAZETTEER_MATCH_IGNORABLE);
}

inline bool address_phrase_is_edge_ignorable_for_components(phrase_t phrase, uint32_t address_components) {
    return address_phrase_matches_type_for_components(phrase, address_components, GAZETTEER_MATCH_EDGE_IGNORABLE);
}


inline bool address_phrase_is_possible_root_for_components(phrase_t phrase, uint32_t address_components) {
    return address_phrase_matches_type_for_components(phrase, address_components, GAZETTEER_MATCH_POSSIBLE_ROOT);
}

inline bool address_phrase_is_specifier_for_components(phrase_t phrase, uint32_t address_components) {
    return address_phrase_matches_type_for_components(phrase, address_components, GAZETTEER_MATCH_SPECIFIER);
}

inline bool address_phrase_is_valid_for_components(phrase_t phrase, uint32_t address_components) {
    return address_phrase_matches_type_for_components(phrase, address_components, GAZETTEER_MATCH_VALID_COMPONENTS);
}


bool address_phrase_contains_unambiguous_expansion(phrase_t phrase) {
    address_expansion_value_t *value = address_dictionary_get_expansions(phrase.data);
    if (value == NULL) return false;

    address_expansion_array *expansions = value->expansions;
    if (expansions == NULL) return false;

    address_expansion_t *expansions_array = expansions->a;

    for (size_t i = 0; i < expansions->n; i++) {
        address_expansion_t expansion = expansions_array[i];
        if (!address_expansion_in_dictionary(expansion, DICTIONARY_AMBIGUOUS_EXPANSION)) {
            return true;
        }
    }
    return false;
}

string_tree_t *add_string_alternatives_phrase_option(char *str, libpostal_normalize_options_t options, expansion_phrase_option_t phrase_option) {
    char_array *key = NULL;

    log_debug("input=%s\n", str);
    token_array *token_array = tokenize_keep_whitespace(str);

    if (token_array == NULL) {
        return NULL;
    }

    size_t len = strlen(str);

    token_t *tokens = token_array->a;
    size_t num_tokens = token_array->n;

    log_debug("tokenized, num tokens=%zu\n", num_tokens);

    bool last_was_punctuation = false;

    phrase_language_array *phrases = NULL;
    phrase_array *lang_phrases = NULL;

    for (size_t i = 0; i < options.num_languages; i++)  {
        char *lang = options.languages[i];
        log_debug("lang=%s\n", lang);

        lang_phrases = search_address_dictionaries_tokens(str, token_array, lang);
        
        if (lang_phrases == NULL) { 
            log_debug("lang_phrases NULL\n");
            continue;
        }

        log_debug("lang_phrases->n = %zu\n", lang_phrases->n);

        phrases = phrases != NULL ? phrases : phrase_language_array_new_size(lang_phrases->n);

        for (size_t j = 0; j < lang_phrases->n; j++) {
            phrase_t p = lang_phrases->a[j];
            log_debug("lang=%s, (%d, %d)\n", lang, p.start, p.len);
            phrase_language_array_push(phrases, (phrase_language_t){lang, p});
        }

        phrase_array_destroy(lang_phrases);
    }


    lang_phrases = search_address_dictionaries_tokens(str, token_array, ALL_LANGUAGES);
    if (lang_phrases != NULL) {
        phrases = phrases != NULL ? phrases : phrase_language_array_new_size(lang_phrases->n);

        for (size_t j = 0; j < lang_phrases->n; j++) {
            phrase_t p = lang_phrases->a[j];
            phrase_language_array_push(phrases, (phrase_language_t){ALL_LANGUAGES, p});
        }
        phrase_array_destroy(lang_phrases);

    }

    string_tree_t *tree = string_tree_new_size(len);

    bool last_added_was_whitespace = false;

    uint64_t normalize_string_options = get_normalize_string_options(options);

    if (phrases != NULL) {
        log_debug("phrases not NULL, n=%zu\n", phrases->n);
        ks_introsort(phrase_language_array, phrases->n, phrases->a);

        phrase_language_t phrase_lang;

        size_t start = 0;
        size_t end = 0;

        phrase_t phrase = NULL_PHRASE;
        phrase_t prev_phrase = NULL_PHRASE;

        key = key != NULL ? key : char_array_new_size(DEFAULT_KEY_LEN);

        log_debug("phrase_option = %d\n", phrase_option);

        bool delete_phrases = phrase_option == DELETE_PHRASES;
        bool expand_phrases = phrase_option == EXPAND_PHRASES;

        size_t num_phrases = phrases->n;

        bool have_non_phrase_tokens = false;
        bool have_non_phrase_word_tokens = false;
        bool have_canonical_phrases = false;
        bool have_ambiguous = false;
        bool have_possible_root = false;
        bool have_strictly_ignorable = false;
        bool have_strictly_ignorable_abbreviation = false;

        size_t prev_phrase_end = 0;

        if (delete_phrases) {
            for (size_t i = 0; i < num_phrases; i++) {
                phrase_lang = phrases->a[i];
                phrase = phrase_lang.phrase;

                log_debug("phrase.start = %zu, prev_phrase_end = %zu\n", phrase.start, prev_phrase_end);

                token_t inter_token;
                if (phrase.start > prev_phrase_end) {
                    for (size_t j = prev_phrase_end; j < phrase.start; j++) {
                        inter_token = tokens[j];
                        if (!is_punctuation(inter_token.type) && !is_whitespace(inter_token.type)) {
                            log_debug("have_non_phrase_tokens\n");
                            have_non_phrase_tokens = true;
                            have_non_phrase_word_tokens = have_non_phrase_word_tokens || is_word_token(inter_token.type);
                            break;
                        }
                    }
                }

                if (i == num_phrases - 1 && phrase.start + phrase.len < num_tokens) {
                    for (size_t j = phrase.start + phrase.len; j < num_tokens; j++) {
                        inter_token = tokens[j];
                        if (!is_punctuation(inter_token.type) && !is_whitespace(inter_token.type)) {
                            have_non_phrase_tokens = true;
                            have_non_phrase_word_tokens = have_non_phrase_word_tokens || is_word_token(inter_token.type);
                            break;
                        }
                    }
                }

                bool phrase_is_ambiguous = address_phrase_in_dictionary(phrase, DICTIONARY_AMBIGUOUS_EXPANSION);
                bool phrase_is_strictly_ignorable = address_phrase_is_ignorable_for_components(phrase, options.address_components) && !phrase_is_ambiguous;
                bool phrase_is_canonical = address_phrase_has_canonical_interpretation(phrase);

                have_non_phrase_tokens = have_non_phrase_tokens || (!phrase_is_strictly_ignorable && !phrase_is_ambiguous);
                log_debug("have_non_phrase_word_tokens = %d, phrase_is_strictly_ignorable = %d, phrase_is_ambiguous = %d\n", have_non_phrase_word_tokens, phrase_is_strictly_ignorable, phrase_is_ambiguous);
                if (!have_non_phrase_word_tokens && !phrase_is_strictly_ignorable && !phrase_is_ambiguous) {
                    for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                        token_t pt = tokens[j];
                        if (is_word_token(pt.type)) {
                            log_debug("have_non_phrase_word_tokens\n");
                            have_non_phrase_word_tokens = true;
                            break;
                        }
                    }
                }


                have_strictly_ignorable = have_strictly_ignorable || phrase_is_strictly_ignorable;
                have_strictly_ignorable_abbreviation = have_strictly_ignorable_abbreviation || (phrase_is_strictly_ignorable && !phrase_is_canonical);
                if (have_strictly_ignorable_abbreviation) {
                    log_debug("have_strictly_ignorable=%zu, phrase_is_canonical=%zu\n", have_strictly_ignorable, phrase_is_canonical);
                }

                have_possible_root = have_possible_root | address_phrase_is_possible_root_for_components(phrase, options.address_components);

                have_canonical_phrases = have_canonical_phrases || (phrase_is_canonical && !phrase_is_ambiguous);
                have_ambiguous = have_ambiguous || phrase_is_ambiguous;

                prev_phrase_end = phrase.start + phrase.len;
            }


            log_debug("have_non_phrase_tokens = %d\n", have_non_phrase_tokens);
            log_debug("have_canonical_phrases = %d\n", have_canonical_phrases);
            log_debug("have_ambiguous = %d\n", have_ambiguous);
            log_debug("have_strictly_ignorable = %d\n", have_strictly_ignorable);
            log_debug("have_strictly_ignorable_abbreviation = %d\n", have_strictly_ignorable_abbreviation);

        }

        bool skipped_last_edge_phrase = false;

        for (size_t i = 0; i < phrases->n; i++) {
            phrase_lang = phrases->a[i];

            phrase = phrase_lang.phrase;

            log_debug("phrase.start=%d, phrase.len=%d, lang=%s, prev_phrase.start=%d, prev_phrase.len=%d\n", phrase.start, phrase.len, phrase_lang.language, prev_phrase.start, prev_phrase.len);

            if ((phrase.start > prev_phrase.start && phrase.start < prev_phrase.start + prev_phrase.len) || (phrase.start == prev_phrase.start && i > 0 && phrase.len < prev_phrase.len)) {
                log_debug("continuing\n");
                continue;
            }

            char_array_clear(key);

            char_array_cat(key, phrase_lang.language);
            char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);

            size_t namespace_len = key->n;

            end = phrase.start;

            log_debug("start=%zu, end=%zu\n", start, end);
            for (size_t j = start; j < end; j++) {
                log_debug("Adding token %zu\n", j);
                token_t token = tokens[j];
                if (is_punctuation(token.type)) {
                    last_was_punctuation = true;
                    continue;
                }

                if (token.type != WHITESPACE) {
                    if ((phrase.start > 0 && last_was_punctuation) || (!last_added_was_whitespace && string_tree_num_tokens(tree) > 0) || (prev_phrase.start == phrase.start && prev_phrase.len == phrase.len) ) {
                        log_debug("Adding space\n");
                        string_tree_add_string(tree, " ");
                        string_tree_finalize_token(tree);
                    }
                    log_debug("Adding previous token, %.*s\n", (int)token.len, str + token.offset);

                    bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                    string_tree_finalize_token(tree);
                    last_added_was_whitespace = false;
                } else if (!delete_phrases && !last_added_was_whitespace && string_tree_num_tokens(tree) > 0 ) {
                    log_debug("Adding pre-phrase whitespace\n");
                    last_added_was_whitespace = true;
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                } else {
                    continue;
                }

                last_was_punctuation = false;
            }

            size_t added_expansions = 0;
            token_t token;

            uint32_t expansion_index = phrase.data;
            address_expansion_value_t *value = address_dictionary_get_expansions(expansion_index);

            bool expansion_valid_components = (value->components & options.address_components) || address_phrase_is_valid_for_components(phrase, options.address_components);

            bool is_numeric_component = (value->components & options.address_components & NUMERIC_ADDRESS_COMPONENTS);

            if (expansion_valid_components) {
                key->n = namespace_len;
                for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens[j];
                    if (token.type != WHITESPACE) {
                        char_array_cat_len(key, str + token.offset, token.len);
                        last_added_was_whitespace = false;
                    } else if (!last_added_was_whitespace) {
                        char_array_cat(key, " ");
                        last_added_was_whitespace = true;
                    }
                }

                char *key_str = char_array_get_string(key);
                log_debug("key_str=%s\n", key_str);
                address_expansion_array *expansions = value->expansions;

                if (expansions != NULL) {
                    bool current_phrase_have_ambiguous = delete_phrases && address_phrase_in_dictionary(phrase, DICTIONARY_AMBIGUOUS_EXPANSION);
                    bool added_pre_phrase_space = false;
                    bool current_phrase_have_ignorable = delete_phrases && address_phrase_is_ignorable_for_components(phrase, options.address_components);
                    bool current_phrase_have_edge_ignorable = false;

                    bool current_phrase_have_specifier = delete_phrases && address_phrase_is_specifier_for_components(phrase, options.address_components);
                    bool current_phrase_have_canonical = delete_phrases && address_phrase_has_canonical_interpretation(phrase);
                    bool current_phrase_have_possible_root = delete_phrases && address_phrase_is_possible_root_for_components(phrase, options.address_components);

                    bool current_phrase_have_valid = address_phrase_is_valid_for_components(phrase, options.address_components);

                    log_debug("current_phrase_have_specifier = %d\n", current_phrase_have_specifier);

                    bool current_phrase_have_unambiguous = delete_phrases && address_phrase_contains_unambiguous_expansion(phrase);

                    /*
                    Edge phrase handling. This is primarily for handling pre-directionals/post-directionals
                    in English and other languages.
                    */
                    bool skip_edge_phrase = false;
                    bool other_phrase_is_ignorable = false;

                    if (delete_phrases) {
                        phrase_language_t other_phrase_lang;
                        phrase_t other_phrase;

                        log_debug("i = %zu, phrase.start = %u\n", i, phrase.start);
                        if (i == 0 && phrase.start == 0 && phrase.start + phrase.len < num_tokens) {
                            current_phrase_have_edge_ignorable = address_phrase_is_edge_ignorable_for_components(phrase, options.address_components);
                            // Delete "E" in "E 125th St"
                            if (current_phrase_have_edge_ignorable) {
                                log_debug("edge-ignorable phrase [%u, %u]\n", phrase.start, phrase.start + phrase.len);
                                skip_edge_phrase = true;
                            }

                            if (!skip_edge_phrase || !have_non_phrase_tokens) {
                                for (size_t other_i = i + 1; other_i < phrases->n; other_i++) {
                                    other_phrase_lang = phrases->a[other_i];
                                    other_phrase = other_phrase_lang.phrase;
                                    log_debug("phrase.start + phrase.len = %u\n", phrase.start + phrase.len);
                                    log_debug("other_phrase.start = %u, other_phrase.len = %u, lang=%s\n", other_phrase.start, other_phrase.len, other_phrase_lang.language);
                                    if (other_phrase.start >= phrase.start + phrase.len && string_equals(other_phrase_lang.language, phrase_lang.language)) {
                                        if (other_phrase.start + other_phrase.len == num_tokens) {
                                            skip_edge_phrase = false;
                                            if (current_phrase_have_edge_ignorable || (current_phrase_have_ambiguous && current_phrase_have_canonical)) {
                                                // don't delete the "E" in "E St"
                                                log_debug("initial phrase is edge ignorable out of two phrases. Checking next phrase is ignorable.\n");

                                                skip_edge_phrase = !(address_phrase_is_ignorable_for_components(other_phrase, options.address_components) && !(address_phrase_has_canonical_interpretation(other_phrase) && address_phrase_is_possible_root_for_components(other_phrase, options.address_components)));
                                                log_debug("skip_edge_phrase = %d\n", skip_edge_phrase);
                                            } else {
                                                log_debug("initial phrase is not edge-ignorable out of two phrases. Checking next phrase is edge ignorable.\n");
                                                // delete "Avenue" in "Avenue E"
                                                other_phrase_is_ignorable = address_phrase_is_edge_ignorable_for_components(other_phrase, options.address_components) || (address_phrase_in_dictionary(other_phrase, DICTIONARY_AMBIGUOUS_EXPANSION) && address_phrase_has_canonical_interpretation(other_phrase));
                                                skip_edge_phrase = other_phrase_is_ignorable && address_phrase_is_ignorable_for_components(phrase, options.address_components) && !(address_phrase_has_canonical_interpretation(phrase) && address_phrase_is_possible_root_for_components(phrase, options.address_components));
                                                
                                            }
                                        } else {
                                            // If we encounter an ignorable phrase like St and we're _not_ the end of the string e.g. "E St SE", the first token is probably a legit token instead of a pre-directional
                                            skip_edge_phrase = !(address_phrase_is_ignorable_for_components(other_phrase, options.address_components) && !((address_phrase_has_canonical_interpretation(other_phrase) || address_phrase_is_edge_ignorable_for_components(other_phrase, options.address_components)) && address_phrase_is_possible_root_for_components(other_phrase, options.address_components)));
                                            log_debug("phrase is possible root. skip_edge_phrase = %d\n", skip_edge_phrase);
                                        }
                                        break;
                                    }
                                }
                            }
                        } else if (phrases->n > 1 && i == phrases->n - 1 && phrase.start + phrase.len == num_tokens && phrase.start > 0) {
                            current_phrase_have_edge_ignorable = address_phrase_is_edge_ignorable_for_components(phrase, options.address_components);
                            if (current_phrase_have_edge_ignorable) {
                                log_debug("edge-ignorable phrase [%u, %u]\n", phrase.start, phrase.start + phrase.len);
                                skip_edge_phrase = true;
                            }

                            log_debug("have_non_phrase_tokens = %d\n", have_non_phrase_tokens);
                            if (!skip_edge_phrase || !have_non_phrase_tokens) {
                                for (ssize_t other_j = i - 1; other_j >= 0; other_j--) {
                                    other_phrase_lang = phrases->a[other_j];
                                    other_phrase = other_phrase_lang.phrase;
                                    log_debug("phrase.start + phrase.len = %u\n", phrase.start + phrase.len);
                                    log_debug("other_phrase.start = %u, other_phrase.len = %u, lang=%s\n", other_phrase.start, other_phrase.len, other_phrase_lang.language);
                                    if (other_phrase.start + other_phrase.len <= phrase.start && string_equals(other_phrase_lang.language, phrase_lang.language)) {
                                        if (other_phrase.start == 0) {
                                            //other_phrase_invalid = address_phrase_is_ignorable_for_components(other_phrase, options.address_components) && !address_phrase_has_canonical_interpretation(other_phrase) && !address_phrase_is_possible_root_for_components(other_phrase, options.address_components);
                                            skip_edge_phrase = false;
                                            if (current_phrase_have_edge_ignorable || (current_phrase_have_ambiguous && current_phrase_have_canonical)) {
                                                // don't delete the "E" in "Avenue E"
                                                log_debug("final phrase is edge ignorable out of two phrases. Checking previous phrase is ignorable.\n");

                                                skip_edge_phrase = !(address_phrase_is_ignorable_for_components(other_phrase, options.address_components) && !(address_phrase_has_canonical_interpretation(other_phrase) && address_phrase_is_possible_root_for_components(other_phrase, options.address_components))) && string_tree_num_tokens(tree) > 0;
                                            } else {
                                                log_debug("final phrase is not edge-ignorable out of two phrases. Checking previous phrase is edge ignorable.\n");
                                                // delete "St" in "E St"
                                                other_phrase_is_ignorable = address_phrase_is_edge_ignorable_for_components(other_phrase, options.address_components) || (address_phrase_in_dictionary(other_phrase, DICTIONARY_AMBIGUOUS_EXPANSION) && address_phrase_has_canonical_interpretation(other_phrase));
                                                skip_edge_phrase = other_phrase_is_ignorable && address_phrase_is_ignorable_for_components(phrase, options.address_components) && !(address_phrase_has_canonical_interpretation(phrase) && address_phrase_is_possible_root_for_components(phrase, options.address_components));

                                                //skip_edge_phrase = address_phrase_is_edge_ignorable_for_components(other_phrase, options.address_components);
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (phrase.start == prev_phrase.start && phrase.len == prev_phrase.len && skipped_last_edge_phrase) {
                        skip_edge_phrase = true;
                    }

                    for (size_t j = 0; j < expansions->n; j++) {
                        if (skip_edge_phrase) {
                            skipped_last_edge_phrase = true;
                            log_debug("skip edge phrase\n");
                            continue;
                        } else {
                            skipped_last_edge_phrase = false;
                        }

                        address_expansion_t expansion = expansions->a[j];

                        bool current_phrase_ignorable = false;
                        bool current_phrase_expandable = expand_phrases && expansion.canonical_index != NULL_CANONICAL_INDEX;

                        bool is_ambiguous = address_expansion_in_dictionary(expansion, DICTIONARY_AMBIGUOUS_EXPANSION);
                        bool is_valid_for_components = address_expansion_is_valid_for_components(expansion, options.address_components);

                        if (delete_phrases) {
                            bool is_ignorable = address_expansion_is_ignorable_for_components(expansion, options.address_components);
                            bool is_canonical = expansion.canonical_index == NULL_CANONICAL_INDEX;

                            log_debug("is_ignorable = %d, is_canonical = %d, is_ambiguous = %d, current_phrase_have_ambiguous = %d, current_phrase_have_unambiguous = %d, have_strictly_ignorable = %d, current_phrase_have_ignorable=%d, current_phrase_have_possible_root=%d\n", is_ignorable, is_canonical, is_ambiguous, current_phrase_have_ambiguous, current_phrase_have_unambiguous, have_strictly_ignorable, current_phrase_have_ignorable, current_phrase_have_possible_root);

                            current_phrase_expandable = current_phrase_expandable || current_phrase_have_ambiguous;

                            if (!is_canonical) {
                                char *canon = address_dictionary_get_canonical(expansion.canonical_index);
                                log_debug("canonical = %s\n", canon);
                            }

                            // Edge phrase calculations from above
                            if (current_phrase_have_edge_ignorable || other_phrase_is_ignorable) {
                                log_debug("current_phrase_have_edge_ignorable\n");
                                log_debug("skip_edge_phrase = %d\n", skip_edge_phrase);
                                current_phrase_ignorable = skip_edge_phrase;
                            // Don't delete "PH" in "PH 1" for unit expansions
                            } else if (is_ignorable && current_phrase_have_specifier) {
                                log_debug("current_phrase_have_specifier\n");
                                current_phrase_ignorable = false;
                            // Delete "Avenue" in "5th Avenue"
                            } else if (is_ignorable && is_canonical && !current_phrase_have_ambiguous) {
                                log_debug("is_ignorable && is_canonical && !current_phrase_have_ambiguous\n");
                                current_phrase_ignorable = have_non_phrase_tokens || (have_possible_root && !current_phrase_have_possible_root) || string_tree_num_tokens(tree) > 0;
                                log_debug("current_phrase_ignorable = %d\n", current_phrase_ignorable);
                            // Delete "Ave" in "5th Ave" or "Pl" in "Park Pl S"
                            } else if (is_ignorable && !is_canonical && !is_ambiguous && !current_phrase_have_ambiguous) {
                                log_debug("is_ignorable && !is_canonical && !current_phrase_have_ambiguous\n");
                                current_phrase_ignorable = have_non_phrase_tokens || (have_possible_root && !current_phrase_have_possible_root) || string_tree_num_tokens(tree) > 0;
                                log_debug("current_phrase_ignorable = %d\n", current_phrase_ignorable);
                            } else if (current_phrase_have_ambiguous && (have_non_phrase_word_tokens || is_numeric_component || have_canonical_phrases || have_possible_root)) {
                                log_debug("current_phrase_have_ambiguous && have_non_phrase_tokens = %d, have_canonical_phrases = %d, have_possible_root = %d, have_non_phrase_word_tokens = %d, is_numeric_component = %d, have_non_phrase_tokens = %d\n", have_non_phrase_tokens, have_canonical_phrases, have_possible_root, have_non_phrase_word_tokens, is_numeric_component, have_non_phrase_tokens);
                                current_phrase_ignorable = (is_ignorable && !(have_possible_root && !current_phrase_have_possible_root)) || (current_phrase_have_ambiguous && (have_non_phrase_word_tokens || (is_numeric_component && have_non_phrase_tokens)) && current_phrase_have_ignorable && current_phrase_have_unambiguous);
                                log_debug("current_phrase_ignorable = %d\n", current_phrase_ignorable);
                            } else if (!is_valid_for_components && !is_ambiguous) {
                                log_debug("!is_valid_for_components\n");
                                current_phrase_ignorable = current_phrase_have_ignorable || current_phrase_have_valid;
                                log_debug("current_phrase_ignorable = %d\n", current_phrase_ignorable);
                            } else {
                                log_debug("none of the above\n");
                            }

                            if (!current_phrase_ignorable && !last_added_was_whitespace && string_tree_num_tokens(tree) > 0 && !added_pre_phrase_space) {
                                log_debug("Adding space\n");
                                string_tree_add_string(tree, " ");
                                string_tree_finalize_token(tree);
                                last_added_was_whitespace = true;
                                added_pre_phrase_space = true;
                            }

                        }

                        if (current_phrase_ignorable) {
                            continue;
                        }

                        if (delete_phrases) {
                            current_phrase_expandable = !current_phrase_ignorable;
                        } else {
                            current_phrase_expandable = (expansion.address_components & options.address_components) || is_valid_for_components;
                        }

                        log_debug("current_phrase_expandable = %d\n", current_phrase_expandable);

                        log_debug("expansion.canonical_index = %d\n", expansion.canonical_index);

                        if (expansion.canonical_index != NULL_CANONICAL_INDEX && current_phrase_expandable) {
                            log_debug("expansion.canonical_index != NULL_CANONICAL_INDEX, delete_phrases = %d, phrase_option = %d\n", delete_phrases, phrase_option);
                            char *canonical = address_dictionary_get_canonical(expansion.canonical_index);
                            char *canonical_normalized = normalize_string_latin(canonical, strlen(canonical), normalize_string_options);

                            canonical = canonical_normalized != NULL ? canonical_normalized : canonical;

                            if (phrase.start + phrase.len < num_tokens - 1) {
                                token_t next_token = tokens[phrase.start + phrase.len];
                                if (!is_numeric_token(next_token.type)) {
                                    log_debug("non-canonical phrase, adding canonical string: %s\n", canonical);
                                    string_tree_add_string(tree, canonical);
                                    last_added_was_whitespace = false;
                                } else {
                                    log_debug("adding canonical with cstring_array methods: %s\n", canonical);
                                    uint32_t start_index = cstring_array_start_token(tree->strings);
                                    cstring_array_append_string(tree->strings, canonical);
                                    cstring_array_append_string(tree->strings, " ");
                                    last_added_was_whitespace = true;
                                    cstring_array_terminate(tree->strings);
                                }
                            } else {
                                log_debug("adding canonical: %s\n", canonical);
                                string_tree_add_string(tree, canonical);
                                last_added_was_whitespace = false;
                            }

                            if (canonical_normalized != NULL) {
                                free(canonical_normalized);
                            }
                        } else if (expansion.canonical_index == NULL_CANONICAL_INDEX || !current_phrase_expandable) {
                            log_debug("canonical phrase, adding canonical string\n");

                            uint32_t start_index = cstring_array_start_token(tree->strings);
                            for (size_t k = phrase.start; k < phrase.start + phrase.len; k++) {
                                token = tokens[k];
                                if (token.type != WHITESPACE) {
                                    cstring_array_append_string_len(tree->strings, str + token.offset, token.len);
                                    last_added_was_whitespace = false;
                                } else {
                                    log_debug("space\n");
                                    cstring_array_append_string(tree->strings, " ");
                                    last_added_was_whitespace = true;
                                }
                            }
                            cstring_array_terminate(tree->strings);
                        } else {
                            continue;
                        }

                        added_expansions++;
                    }

                }
            }

            log_debug("expansion_valid_components == %d\n", expansion_valid_components);

            if (added_expansions == 0 && (!delete_phrases || !expansion_valid_components)) {
                if (!last_added_was_whitespace && string_tree_num_tokens(tree) > 0) {
                    log_debug("Adding space\n");
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                    last_added_was_whitespace = true;
                }

                uint32_t start_index = cstring_array_start_token(tree->strings);

                for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens[j];

                    if (token.type != WHITESPACE) {
                        log_debug("Adding canonical token, %.*s\n", (int)token.len, str + token.offset);
                        cstring_array_append_string_len(tree->strings, str + token.offset, token.len);
                        last_added_was_whitespace = false;
                    } else if (!last_added_was_whitespace) {
                        log_debug("Adding space\n");
                        cstring_array_append_string(tree->strings, " ");
                        last_added_was_whitespace = true;
                    }

                }

                cstring_array_terminate(tree->strings);

            }

            if (!delete_phrases || !expansion_valid_components || added_expansions > 0) {
                log_debug("i=%zu\n", i);
                bool end_of_phrase = false;
                if (i < phrases->n - 1) {
                    phrase_t next_phrase = phrases->a[i + 1].phrase;
                    end_of_phrase = (next_phrase.start != phrase.start || next_phrase.len != phrase.len);
                } else {
                    end_of_phrase = true;
                }

                log_debug("end_of_phrase=%d\n", end_of_phrase);
                if (end_of_phrase) {                
                    log_debug("finalize at i=%zu\n", i);
                    string_tree_finalize_token(tree);
                }
            }

            start = phrase.start + phrase.len;
            prev_phrase = phrase;

        }

        char_array_destroy(key);

        end = (int)num_tokens;

        if (phrase.start + phrase.len > 0 && phrase.start + phrase.len <= end - 1 && !last_added_was_whitespace) {
            token_t next_token = tokens[phrase.start + phrase.len];
            if (next_token.type != WHITESPACE && !last_added_was_whitespace && string_tree_num_tokens(tree) > 0 && !is_ideographic(next_token.type)) {
                log_debug("space after phrase\n");
                string_tree_add_string(tree, " ");
                last_added_was_whitespace = true;
                string_tree_finalize_token(tree);
            }
        }


        for (size_t j = start; j < end; j++) {
            log_debug("On token %zu\n", j);
            token_t token = tokens[j];
            if (is_punctuation(token.type)) {
                log_debug("last_was_punctuation\n");
                last_was_punctuation = true;
                continue;
            }

            if (token.type != WHITESPACE) {
                if (j > 0 && last_was_punctuation && !last_added_was_whitespace && string_tree_num_tokens(tree) > 0) {
                    log_debug("Adding another space\n");
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                }
                log_debug("Adding previous token, %.*s\n", (int)token.len, str + token.offset);

                bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                last_added_was_whitespace = false;
            } else if (!last_added_was_whitespace && string_tree_num_tokens(tree) > 0) {
                log_debug("Adding space IV\n");
                string_tree_add_string(tree, " ");
                last_added_was_whitespace = true;
            } else {
                log_debug("Skipping token %zu\n", j);
                continue;
            }

            last_was_punctuation = false;
            string_tree_finalize_token(tree);

        }

    } else {
        log_debug("phrases NULL\n");
        for (size_t j = 0; j < num_tokens; j++) {
            log_debug("On token %zu\n", j);
            token_t token = tokens[j];
            if (is_punctuation(token.type)) {
                log_debug("punctuation, skipping\n");
                last_was_punctuation = true;
                continue;
            }

            if (token.type != WHITESPACE) {
                if (last_was_punctuation && !last_added_was_whitespace && string_tree_num_tokens(tree) > 0) {
                    log_debug("Adding space V\n");
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                }

                bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                last_added_was_whitespace = false;
            } else if (!last_added_was_whitespace && string_tree_num_tokens(tree) > 0) {
                log_debug("Adding space VI\n");
                string_tree_add_string(tree, " ");
                last_added_was_whitespace = true;
            } else {
                continue;
            }

            last_was_punctuation = false;
            string_tree_finalize_token(tree);
        }
    }

    if (phrases != NULL) {
        phrase_language_array_destroy(phrases);
    }

    token_array_destroy(token_array);

    return tree;
}

inline bool normalize_ordinal_suffixes(string_tree_t *tree, char *str, char *lang, token_t token, size_t i, token_t prev_token, libpostal_normalize_options_t options) {
    size_t len_ordinal_suffix = valid_ordinal_suffix_len(str, token, prev_token, lang);

    if (len_ordinal_suffix > 0) {
        cstring_array *strings = tree->strings;
        // Add the original form first. When this function returns true,
        // add_normalized_strings_token won't be called a second time.
        add_normalized_strings_token(strings, str, token, options);
        token_t normalized_token = token;
        normalized_token.len = token.len - len_ordinal_suffix;
        add_normalized_strings_token(strings, str, normalized_token, options);
        return true;
    }

    return false;
}

inline void add_normalized_strings_tokenized(string_tree_t *tree, char *str, token_array *tokens, libpostal_normalize_options_t options) {
    cstring_array *strings = tree->strings;

    token_t prev_token = (token_t){0, 0, 0};

    for (size_t i = 0; i < tokens->n; i++) {
        token_t token = tokens->a[i];
        bool have_phrase = false;
        bool have_ordinal = false;

        if (is_special_token(token.type)) {
            string_tree_add_string_len(tree, str + token.offset, token.len);
            string_tree_finalize_token(tree);
            continue;
        }

        for (size_t j = 0; j < options.num_languages; j++) {
            char *lang = options.languages[j];
            if (expand_affixes(tree, str, lang, token, options)) {
                have_phrase = true;
                break;
            }

            if (normalize_ordinal_suffixes(tree, str, lang, token, i, prev_token, options)) {
                have_ordinal = true;
                break;
            }
        }

        if (!have_phrase && !have_ordinal) {
            add_normalized_strings_token(strings, str, token, options);
        }

        string_tree_finalize_token(tree);
        prev_token = token;
    }

}


void expand_alternative_phrase_option(cstring_array *strings, khash_t(str_set) *unique_strings, char *str, libpostal_normalize_options_t options, expansion_phrase_option_t phrase_option) {
    size_t len = strlen(str);
    token_array *tokens = tokenize_keep_whitespace(str);
    string_tree_t *token_tree = string_tree_new_size(len);

    add_normalized_strings_tokenized(token_tree, str, tokens, options);

    string_tree_iterator_t *tokenized_iter = string_tree_iterator_new(token_tree);

    string_tree_iterator_t *iter;

    char_array *temp_string = char_array_new_size(len);

    char *token;

    char *lang;

    kh_resize(str_set, unique_strings, kh_size(unique_strings) + tokenized_iter->remaining);

    bool excessive_perms_outer = tokenized_iter->remaining >= EXCESSIVE_PERMUTATIONS;

    if (!excessive_perms_outer) {
        kh_resize(str_set, unique_strings, kh_size(unique_strings) + tokenized_iter->remaining);
    }

    log_debug("tokenized_iter->remaining=%d\n", tokenized_iter->remaining);

    for (; !string_tree_iterator_done(tokenized_iter); string_tree_iterator_next(tokenized_iter)) {
        char_array_clear(temp_string);

        string_tree_iterator_foreach_token(tokenized_iter, token, {
            if (token == NULL) {
                continue;
            }
            char_array_append(temp_string, token);
        })
        char_array_terminate(temp_string);

        char *tokenized_str = char_array_get_string(temp_string);
        
        string_tree_t *alternatives;

        int ret;
        log_debug("Adding alternatives for single normalization\n");
        alternatives = add_string_alternatives_phrase_option(tokenized_str, options, phrase_option);

        log_debug("num strings = %" PRIu32 "\n", string_tree_num_strings(alternatives));

        if (alternatives == NULL) {
            log_debug("alternatives = NULL\n");
            continue;
        }

        iter = string_tree_iterator_new(alternatives);
        log_debug("iter->num_tokens=%d\n", iter->num_tokens);
        log_debug("iter->remaining=%d\n", iter->remaining);

        bool excessive_perms_inner = iter->remaining >= EXCESSIVE_PERMUTATIONS;

        if (!excessive_perms_inner && !excessive_perms_outer) {
            for (; !string_tree_iterator_done(iter); string_tree_iterator_next(iter)) {
                char_array_clear(temp_string);
                string_tree_iterator_foreach_token(iter, token, {
                    if (token == NULL) {
                        log_debug("token=NULL\n");
                    } else {
                        log_debug("token=%s\n", token);
                        char_array_append(temp_string, token);
                    }
                })
                char_array_terminate(temp_string);

                token = char_array_get_string(temp_string);

                size_t token_len = strlen(token);

                if (token_len == 0) continue;

                size_t left_spaces = string_left_spaces_len(token, token_len);
                size_t right_spaces = string_right_spaces_len(token, token_len);

                if (left_spaces + right_spaces == token_len) {
                    continue;
                }

                char *dupe_token = strndup(token + left_spaces, token_len - left_spaces - right_spaces);

                log_debug("full string=%s\n", token);
                khiter_t k = kh_get(str_set, unique_strings, dupe_token);

                if (k == kh_end(unique_strings)) {
                    log_debug("doing postprocessing\n");
                    add_postprocessed_string(strings, dupe_token, options);
                    k = kh_put(str_set, unique_strings, dupe_token, &ret);
                } else {
                    free(dupe_token);
                }

                log_debug("iter->remaining = %d\n", iter->remaining);

            }
        } else {
            cstring_array_add_string(strings, tokenized_str);
        }

        string_tree_iterator_destroy(iter);
        string_tree_destroy(alternatives);

        if (excessive_perms_outer) {
            break;
        }
    }

    string_tree_iterator_destroy(tokenized_iter);
    string_tree_destroy(token_tree);

    token_array_destroy(tokens);

    char_array_destroy(temp_string);
}



void expand_alternative_phrase_option_languages(cstring_array *strings, khash_t(str_set) *unique_strings, char *str, libpostal_normalize_options_t options, expansion_phrase_option_t phrase_option) {
    char **temp_languages = calloc(1, sizeof(char *));
    libpostal_normalize_options_t temp_options = options;

    for (size_t i = 0; i < options.num_languages; i++) {
        char *lang = options.languages[i];

        temp_languages[0] = lang;
        temp_options.languages = temp_languages;
        temp_options.num_languages = 1;
        expand_alternative_phrase_option(strings, unique_strings, str, temp_options, phrase_option);
    }

    if (options.num_languages == 0) {
        temp_options.languages = options.languages;
        temp_options.num_languages = options.num_languages;
        expand_alternative_phrase_option(strings, unique_strings, str, temp_options, phrase_option);
    }

    free(temp_languages);
}


cstring_array *expand_address_phrase_option(char *input, libpostal_normalize_options_t options, size_t *n, expansion_phrase_option_t phrase_option) {
    options.address_components |= LIBPOSTAL_ADDRESS_ANY;

    uint64_t normalize_string_options = get_normalize_string_options(options);

    size_t len = strlen(input);

    language_classifier_response_t *lang_response = NULL;

    if (options.num_languages == 0) {
         lang_response = classify_languages(input);
         if (lang_response != NULL) {
            options.num_languages = lang_response->num_languages;
            options.languages = lang_response->languages;
         }
    }

    string_tree_t *tree = normalize_string_languages(input, normalize_string_options, options.num_languages, options.languages);

    cstring_array *strings = cstring_array_new_size(len * 2);
    char_array *temp_string = char_array_new_size(len);

    khash_t(str_set) *unique_strings = kh_init(str_set);

    char *token;

    log_debug("string_tree_num_tokens(tree) = %d\n", string_tree_num_tokens(tree));

    if (string_tree_num_strings(tree) == 1) {
        char *normalized = string_tree_get_alternative(tree, 0, 0);
        expand_alternative_phrase_option_languages(strings, unique_strings, normalized, options, phrase_option);

    } else {
        log_debug("Adding alternatives for multiple normalizations\n");
        string_tree_iterator_t *iter = string_tree_iterator_new(tree);

        for (; !string_tree_iterator_done(iter); string_tree_iterator_next(iter)) {
            char *segment;
            char_array_clear(temp_string);
            bool is_first = true;

            string_tree_iterator_foreach_token(iter, segment, {
                if (!is_first) {
                    char_array_append(temp_string, " ");
                }
                char_array_append(temp_string, segment);
                is_first = false;
            })
            char_array_terminate(temp_string);
            token = char_array_get_string(temp_string);
            log_debug("current permutation = %s\n", token);
            expand_alternative_phrase_option_languages(strings, unique_strings, token, options, phrase_option);
        }

        string_tree_iterator_destroy(iter);
    }

    char *key_str = NULL;
    for (size_t i = kh_begin(unique_strings); i != kh_end(unique_strings); ++i) {
        if (!kh_exist(unique_strings, i)) continue;
        key_str = (char *)kh_key(unique_strings, i);
        free(key_str);
    }

    kh_destroy(str_set, unique_strings);

    if (lang_response != NULL) {
        language_classifier_response_destroy(lang_response);
    }

    char_array_destroy(temp_string);
    string_tree_destroy(tree);

    *n = cstring_array_num_strings(strings);

    return strings;
}

cstring_array *expand_address(char *input, libpostal_normalize_options_t options, size_t *n) {
    return expand_address_phrase_option(input, options, n, EXPAND_PHRASES);
}

cstring_array *expand_address_root(char *input, libpostal_normalize_options_t options, size_t *n) {
    return expand_address_phrase_option(input, options, n, DELETE_PHRASES);
}


void expansion_array_destroy(char **expansions, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(expansions[i]);
    }
    free(expansions);
}

