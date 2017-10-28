#include <stdlib.h>

#include "libpostal.h"

#include "klib/khash.h"
#include "klib/ksort.h"
#include "log/log.h"

#include "address_dictionary.h"
#include "address_parser.h"
#include "collections.h"
#include "constants.h"
#include "language_classifier.h"
#include "numex.h"
#include "normalize.h"
#include "scanner.h"
#include "string_utils.h"
#include "token_types.h"
#include "transliterate.h"

typedef struct phrase_language {
    char *language;
    phrase_t phrase;
} phrase_language_t;

VECTOR_INIT(phrase_language_array, phrase_language_t)

#define ks_lt_phrase_language(a, b) ((a).phrase.start < (b).phrase.start || ((a).phrase.start == (b).phrase.start && (a).phrase.len > (b).phrase.len))

KSORT_INIT(phrase_language_array, phrase_language_t, ks_lt_phrase_language)

#define DEFAULT_KEY_LEN 32

#define EXCESSIVE_PERMUTATIONS 100

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

static inline uint64_t get_normalize_token_options(libpostal_normalize_options_t options) {
    uint64_t normalize_token_options = 0;

    normalize_token_options |= options.delete_final_periods ? NORMALIZE_TOKEN_DELETE_FINAL_PERIOD : 0;
    normalize_token_options |= options.delete_acronym_periods ? NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS : 0;
    normalize_token_options |= options.drop_english_possessives ? NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES : 0;
    normalize_token_options |= options.delete_apostrophes ? NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE : 0;

    return normalize_token_options;
}

static inline uint64_t get_normalize_string_options(libpostal_normalize_options_t options) {
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


static inline size_t string_hyphen_prefix_len(char *str, size_t len) {
    // Strip beginning hyphens
    int32_t unichr;
    uint8_t *ptr = (uint8_t *)str;
    ssize_t char_len = utf8proc_iterate(ptr, len, &unichr);
    if (utf8_is_hyphen(unichr)) {
        return (size_t)char_len;
    }
    return 0;
}

static inline size_t string_hyphen_suffix_len(char *str, size_t len) {
    // Strip beginning hyphens
    int32_t unichr;
    uint8_t *ptr = (uint8_t *)str;
    ssize_t char_len = utf8proc_iterate_reversed(ptr, len, &unichr);
    if (utf8_is_hyphen(unichr)) {
        return (size_t)char_len;
    }
    return 0;
}

static void add_normalized_strings_token(cstring_array *strings, char *str, token_t token, libpostal_normalize_options_t options) {

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

        if (is_numeric_token(token.type) && options.split_alpha_from_numeric && numeric_starts_with_alpha(str, token)) {
            normalize_token_options |= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
            normalize_token(strings, str, token, normalize_token_options);
            normalize_token_options ^= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
        }
    } else {
        cstring_array_add_string(strings, " ");
    }
}

static void add_postprocessed_string(cstring_array *strings, char *str, libpostal_normalize_options_t options) {
    cstring_array_add_string(strings, str);

    if (options.roman_numerals) {
        char *numex_replaced = replace_numeric_expressions(str, LATIN_LANGUAGE_CODE);
        if (numex_replaced != NULL) {
            cstring_array_add_string(strings, numex_replaced);
            free(numex_replaced);
        }

    }

}



static address_expansion_array *get_affix_expansions(phrase_t phrase, libpostal_normalize_options_t options) {
    uint32_t expansion_index = phrase.data;
    address_expansion_value_t *value = address_dictionary_get_expansions(expansion_index);
    if (value != NULL && value->components & options.address_components) {
        return value->expansions;
    }

    return NULL;
}

static inline void cat_affix_expansion(char_array *key, char *str, address_expansion_t expansion, token_t token, phrase_t phrase, libpostal_normalize_options_t options) {
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


static bool add_affix_expansions(string_tree_t *tree, char *str, char *lang, token_t token, phrase_t prefix, phrase_t suffix, libpostal_normalize_options_t options, bool with_period) {
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
        prefix_expansions = get_affix_expansions(prefix, options);
        if (prefix_expansions == NULL) have_prefix = false;
    }

    if (have_suffix) {
        suffix_expansions = get_affix_expansions(suffix, options);
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

static inline bool expand_affixes(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options) {
    phrase_t suffix = search_address_dictionaries_suffix(str + token.offset, token.len, lang);

    phrase_t prefix = search_address_dictionaries_prefix(str + token.offset, token.len, lang);

    if ((suffix.len == 0 && prefix.len == 0)) return false;

    bool with_period = false;

    return add_affix_expansions(tree, str, lang, token, prefix, suffix, options, with_period);
}

static inline bool expand_affixes_period(string_tree_t *tree, char *str, char *lang, token_t token, libpostal_normalize_options_t options) {
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

static bool add_period_affixes_or_token(string_tree_t *tree, char *str, token_t token, libpostal_normalize_options_t options) {
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


static string_tree_t *add_string_alternatives(char *str, libpostal_normalize_options_t options) {
    char_array *key = NULL;

    log_debug("input=%s\n", str);
    token_array *tokens = tokenize_keep_whitespace(str);

    if (tokens == NULL) {
        return NULL;
    }

    size_t len = strlen(str);

    log_debug("tokenized, num tokens=%zu\n", tokens->n);

    bool last_was_punctuation = false;

    phrase_language_array *phrases = NULL;
    phrase_array *lang_phrases = NULL;


    for (size_t i = 0; i < options.num_languages; i++)  {
        char *lang = options.languages[i];
        log_debug("lang=%s\n", lang);

        lang_phrases = search_address_dictionaries_tokens(str, tokens, lang);
        
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


    lang_phrases = search_address_dictionaries_tokens(str, tokens, ALL_LANGUAGES);
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
                token_t token = tokens->a[j];
                if (is_punctuation(token.type)) {
                    last_was_punctuation = true;
                    continue;
                }

                if (token.type != WHITESPACE) {
                    if (phrase.start > 0 && last_was_punctuation && !last_added_was_whitespace) {
                        string_tree_add_string(tree, " ");
                        string_tree_finalize_token(tree);
                    }
                    log_debug("Adding previous token, %.*s\n", (int)token.len, str + token.offset);

                    bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                    last_added_was_whitespace = false;
                } else if (!last_added_was_whitespace) {
                    log_debug("Adding pre-phrase whitespace\n");
                    last_added_was_whitespace = true;
                    string_tree_add_string(tree, " ");
                } else {
                    continue;
                }

                last_was_punctuation = false;
                string_tree_finalize_token(tree);       
            }

            if (phrase.start > 0 && start < end) {
                token_t prev_token = tokens->a[phrase.start - 1];
                log_debug("last_added_was_whitespace=%d\n", last_added_was_whitespace);
                if (!last_added_was_whitespace && phrase.start - 1 > 0 && (!is_ideographic(prev_token.type) || last_was_punctuation))  {
                    log_debug("Adding space III\n");
                    string_tree_add_string(tree, " ");
                    last_added_was_whitespace = true;
                    string_tree_finalize_token(tree);
                }
            }

            uint32_t expansion_index = phrase.data;
            address_expansion_value_t *value = address_dictionary_get_expansions(expansion_index);

            token_t token;

            size_t added_expansions = 0;
            if ((value->components & options.address_components) > 0) {
                key->n = namespace_len;
                for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens->a[j];
                    if (token.type != WHITESPACE) {
                        char_array_cat_len(key, str + token.offset, token.len);
                        last_added_was_whitespace = false;
                    } else {
                        char_array_cat(key, " ");
                        last_added_was_whitespace = true;
                    }
                }

                char *key_str = char_array_get_string(key);
                log_debug("key_str=%s\n", key_str);
                address_expansion_array *expansions = value->expansions;

                if (expansions != NULL) {
                    for (size_t j = 0; j < expansions->n; j++) {
                        address_expansion_t expansion = expansions->a[j];

                        if ((expansion.address_components & options.address_components) == 0 && !address_expansion_in_dictionary(expansion, DICTIONARY_AMBIGUOUS_EXPANSION)) {
                            continue;
                        }

                        if (expansion.canonical_index != NULL_CANONICAL_INDEX) {
                            char *canonical = address_dictionary_get_canonical(expansion.canonical_index);
                            char *canonical_normalized = normalize_string_latin(canonical, strlen(canonical), normalize_string_options);

                            canonical = canonical_normalized != NULL ? canonical_normalized : canonical;


                            if (phrase.start + phrase.len < tokens->n - 1) {
                                token_t next_token = tokens->a[phrase.start + phrase.len];
                                if (!is_numeric_token(next_token.type)) {
                                    log_debug("non-canonical phrase, adding canonical string\n");
                                    string_tree_add_string(tree, canonical);
                                    last_added_was_whitespace = false;
                                } else {
                                    log_debug("adding canonical with cstring_array methods\n");
                                    uint32_t start_index = cstring_array_start_token(tree->strings);
                                    cstring_array_append_string(tree->strings, canonical);
                                    cstring_array_append_string(tree->strings, " ");
                                    last_added_was_whitespace = true;
                                    cstring_array_terminate(tree->strings);
                                }
                            } else {
                                string_tree_add_string(tree, canonical);
                                last_added_was_whitespace = false;

                            }

                            if (canonical_normalized != NULL) {
                                free(canonical_normalized);
                            }
                        } else {
                            log_debug("canonical phrase, adding canonical string\n");

                            uint32_t start_index = cstring_array_start_token(tree->strings);
                            for (size_t k = phrase.start; k < phrase.start + phrase.len; k++) {
                                token = tokens->a[k];
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
                        }

                        added_expansions++;
                    }


                }
            }

            if (added_expansions == 0) {
                uint32_t start_index = cstring_array_start_token(tree->strings);
                for (size_t j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens->a[j];

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

                if (phrase.start + phrase.len < tokens->n - 1) {
                    token_t next_token = tokens->a[phrase.start + phrase.len + 1];
                    if (next_token.type != WHITESPACE && !last_added_was_whitespace && !is_ideographic(next_token.type)) {
                        cstring_array_append_string(tree->strings, " ");
                        last_added_was_whitespace = true;
                    }
                }

                cstring_array_terminate(tree->strings);

            }

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

            start = phrase.start + phrase.len;
            prev_phrase = phrase;

        }

        char_array_destroy(key);

        end = (int)tokens->n;

        if (phrase.start + phrase.len > 0 && phrase.start + phrase.len <= end - 1) {
            token_t next_token = tokens->a[phrase.start + phrase.len];
            if (next_token.type != WHITESPACE && !last_added_was_whitespace && !is_ideographic(next_token.type)) {
                log_debug("space after phrase\n");
                string_tree_add_string(tree, " ");
                last_added_was_whitespace = true;
                string_tree_finalize_token(tree);
            }
        }


        for (size_t j = start; j < end; j++) {
            log_debug("On token %zu\n", j);
            token_t token = tokens->a[j];
            if (is_punctuation(token.type)) {
                log_debug("last_was_punctuation\n");
                last_was_punctuation = true;
                continue;
            }

            if (token.type != WHITESPACE) {
                if (j > 0 && last_was_punctuation && !last_added_was_whitespace) {
                    log_debug("Adding another space\n");
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                }
                log_debug("Adding previous token, %.*s\n", (int)token.len, str + token.offset);

                bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                last_added_was_whitespace = false;
            } else if (!last_added_was_whitespace) {
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

        for (size_t j = 0; j < tokens->n; j++) {
            log_debug("On token %zu\n", j);
            token_t token = tokens->a[j];
            if (is_punctuation(token.type)) {
                log_debug("punctuation, skipping\n");
                last_was_punctuation = true;
                continue;
            }

            if (token.type != WHITESPACE) {
                if (last_was_punctuation && !last_added_was_whitespace) {
                    log_debug("Adding space V\n");
                    string_tree_add_string(tree, " ");
                    string_tree_finalize_token(tree);
                }

                bool have_period_affixes = add_period_affixes_or_token(tree, str, token, options);
                last_added_was_whitespace = false;
            } else if (!last_added_was_whitespace) {
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

    token_array_destroy(tokens);

    return tree;
}


static inline bool normalize_ordinal_suffixes(string_tree_t *tree, char *str, char *lang, token_t token, size_t i, token_t prev_token, libpostal_normalize_options_t options) {
    size_t len_ordinal_suffix = ordinal_suffix_len(str + token.offset, token.len, lang);

    int32_t unichr = 0;
    const uint8_t *ptr = (const uint8_t *)str;

    if (len_ordinal_suffix > 0) {
        ssize_t start = 0;
        size_t token_offset = token.offset;
        size_t token_len = token.len;

        if (len_ordinal_suffix < token.len) {
            start = token.offset + token.len - len_ordinal_suffix;
            token_offset = token.offset;
            token_len = token.len - len_ordinal_suffix;
        } else {
            start = prev_token.offset + prev_token.len;
            token_offset = prev_token.offset;
            token_len = prev_token.len;
        }
        ssize_t prev_char_len = utf8proc_iterate_reversed(ptr, start, &unichr);
        if (prev_char_len <= 0) return false;
        if (!utf8_is_digit(utf8proc_category(unichr)) && !is_roman_numeral_len(str + token_offset, token_len)) {
            return false;
        }
    } else {
        return false;
    }

    cstring_array *strings = tree->strings;
    // Add the original form first. When this function returns true,
    // add_normalized_strings_token won't be called a second time.
    add_normalized_strings_token(strings, str, token, options);

    token_t normalized_token = token;
    normalized_token.len = token.len - len_ordinal_suffix;
    add_normalized_strings_token(strings, str, normalized_token, options);
    return true;
}

static inline void add_normalized_strings_tokenized(string_tree_t *tree, char *str, token_array *tokens, libpostal_normalize_options_t options) {
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


static void expand_alternative(cstring_array *strings, khash_t(str_set) *unique_strings, char *str, libpostal_normalize_options_t options) {
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
        alternatives = add_string_alternatives(tokenized_str, options);

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
                    log_debug("token=%s\n", token);
                    char_array_append(temp_string, token);
                })
                char_array_terminate(temp_string);

                token = char_array_get_string(temp_string);
                log_debug("full string=%s\n", token);
                khiter_t k = kh_get(str_set, unique_strings, token);

                if (k == kh_end(unique_strings)) {
                    log_debug("doing postprocessing\n");
                    add_postprocessed_string(strings, token, options);
                    k = kh_put(str_set, unique_strings, strdup(token), &ret);
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

char **libpostal_expand_address(char *input, libpostal_normalize_options_t options, size_t *n) {
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
        expand_alternative(strings, unique_strings, normalized, options);

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
            expand_alternative(strings, unique_strings, token, options);
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

    return cstring_array_to_strings(strings);

}

void libpostal_expansion_array_destroy(char **expansions, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(expansions[i]);
    }
    free(expansions);
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
        libpostal_address_parser_response_destroy(parsed);
        return NULL;
    }

    return parsed;
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
