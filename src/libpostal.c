#include "libpostal.h"

#include "klib/ksort.h"
#include "log/log.h"

#include "address_dictionary.h"
#include "collections.h"
#include "geodb.h"
#include "numex.h"
#include "normalize.h"
#include "scanner.h"
#include "transliterate.h"

typedef struct phrase_language {
    char *language;
    phrase_t phrase;
} phrase_language_t;

VECTOR_INIT(phrase_language_array, phrase_language_t)

#define ks_lt_phrase_language(a, b) ((a).phrase.start < (b).phrase.start)

KSORT_INIT(phrase_language_array, phrase_language_t, ks_lt_phrase_language)

#define DEFAULT_KEY_LEN 32

inline bool is_word_token(uint16_t type) {
    return type == WORD || type == ABBREVIATION || type == ACRONYM || type == IDEOGRAPHIC_CHAR || type == HANGUL_SYLLABLE;
}

inline bool is_numeric_token(uint16_t type) {
    return type == NUMERIC;
}

void add_normalized_strings_token(string_tree_t *tree, char *str, token_array *tokens, normalize_options_t options) {
    uint64_t normalize_token_options = 0;

    normalize_token_options |= options.delete_final_periods ? NORMALIZE_TOKEN_DELETE_FINAL_PERIOD : 0;
    normalize_token_options |= options.delete_acronym_periods ? NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS : 0;
    normalize_token_options |= options.drop_english_possessives ? NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES : 0;
    normalize_token_options |= options.delete_apostrophes ? NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE : 0;

    cstring_array *strings = tree->strings;

    for (int i = 0; i < tokens->n; i++) {
        token_t token = tokens->a[i];

        if (token.type != WHITESPACE ) {  
            bool contains_hyphen = string_contains_hyphen_len(str + token.offset, token.len);
            if (!contains_hyphen || token.type == HYPHEN) {
                normalize_token(strings, str, token, normalize_token_options);
            } else if (is_word_token(token.type)) {
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

                if (options.replace_numeric_hyphens) {
                    normalize_token_options |= NORMALIZE_TOKEN_REPLACE_HYPHENS;
                    normalize_token(strings, str, token, normalize_token_options);
                    normalize_token_options ^= NORMALIZE_TOKEN_REPLACE_HYPHENS;
                }

                if (options.delete_numeric_hyphens) {
                    normalize_token_options |= NORMALIZE_TOKEN_DELETE_HYPHENS;
                    normalize_token(strings, str, token, normalize_token_options);
                    normalize_token_options ^= NORMALIZE_TOKEN_DELETE_HYPHENS;
                }
            }
            
            if (is_numeric_token(token.type) && options.split_alpha_from_numeric) {
                normalize_token_options |= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
                normalize_token(strings, str, token, normalize_token_options);
                normalize_token_options ^= NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC;
            }

        } else {
            cstring_array_add_string(strings, " ");
        }

        string_tree_finalize_token(tree);
    }

}

string_tree_t *add_string_alternatives(char *str, normalize_options_t options) {
    char_array *key = NULL;

    log_debug("input=%s\n", str);
    token_array *tokens = tokenize_keep_whitespace(str);

    if (tokens == NULL) {
        return NULL;
    }

    /*

    uint64_t normalize_token_options = 0;

    uint64_t replace_numeric_hyphens:1;
    uint64_t delete_numeric_hyphens:1;
    uint64_t replace_word_hyphens:1;
    uint64_t delete_word_hyphens:1;
    uint64_t delete_final_periods:1;
    uint64_t delete_acronym_periods:1;
    uint64_t drop_english_possessives:1;
    uint64_t delete_apostrophes:1;
    uint64_t expand_numex:1;


    normalize_token_options |= options.replace_numeric_hyphens ? NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS : 0;

    #define NORMALIZE_TOKEN_REPLACE_HYPHENS 1 << 0
    #define NORMALIZE_TOKEN_DELETE_HYPHENS 1 << 1
    #define NORMALIZE_TOKEN_DELETE_FINAL_PERIOD 1 << 2
    #define NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS 1 << 3
    #define NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES 1 << 4
    #define NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE 1 << 5

    char_array *tokens_normalized = normalize_tokens(str, tokens);

    token_array_destroy(tokens);
    free(str);

    str = char_array_to_string(tokens_normalized);
    */

    size_t len = strlen(str);

    log_debug("tokenized, num tokens=%d\n", tokens->n);

    phrase_language_array *phrases = NULL;
    phrase_array *lang_phrases = NULL;

    for (int i = 0; i < options.num_languages; i++)  {
        char *lang = options.languages[i];
        log_debug("lang=%s\n", lang);
        lang_phrases = search_address_dictionaries_tokens(str, tokens, lang);
        
        if (lang_phrases == NULL) { 
            log_debug("lang_phrases NULL\n");
            continue;
        }

        log_debug("lang_phrases->n = %zu\n", lang_phrases->n);

        phrases = phrases != NULL ? phrases : phrase_language_array_new_size(lang_phrases->n);

        for (int j = 0; j < lang_phrases->n; j++) {
            phrase_t p = lang_phrases->a[j];
            phrase_language_array_push(phrases, (phrase_language_t){lang, p});
        }

        phrase_array_destroy(lang_phrases);
    }


    lang_phrases = search_address_dictionaries_tokens(str, tokens, ALL_LANGUAGES);
    if (lang_phrases != NULL) {
        phrases = phrases != NULL ? phrases : phrase_language_array_new_size(lang_phrases->n);

        for (int j = 0; j < lang_phrases->n; j++) {
            phrase_t p = lang_phrases->a[j];
            phrase_language_array_push(phrases, (phrase_language_t){ALL_LANGUAGES, p});
        }
        phrase_array_destroy(lang_phrases);
    }

    string_tree_t *tree = string_tree_new_size(len);

    if (phrases != NULL) {
        log_debug("phrases not NULL, n=%d\n", phrases->n);
        ks_introsort(phrase_language_array, phrases->n, phrases->a);

        phrase_language_t phrase_lang;

        int start = 0;
        int end = 0;

        key = key != NULL ? key : char_array_new_size(DEFAULT_KEY_LEN);

        for (int i = 0; i < phrases->n; i++) {
            phrase_lang = phrases->a[i];
            char_array_clear(key);

            char_array_cat(key, phrase_lang.language);
            char_array_cat(key, NAMESPACE_SEPARATOR_CHAR);

            size_t namespace_len = key->n;

            phrase_t phrase = phrase_lang.phrase;

            end = phrase.start;

            for (int j = start; j < end; j++) {
                token_t token = tokens->a[j]; 
                if (token.type != WHITESPACE) {
                    log_debug("Adding previous token, %.*s\n", token.len, str + token.offset);

                    string_tree_add_string_len(tree, str + token.offset, token.len);
                } else {
                    log_debug("Adding space\n");
                    string_tree_add_string(tree, " ");
                }
                string_tree_finalize_token(tree);       
            }

            expansion_value_t value;
            value.value = phrase.data;

            token_t token;

            if (value.components & options.address_components) {
                key->n = namespace_len;
                for (int j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens->a[j];
                    if (token.type != WHITESPACE) {
                        char_array_cat_len(key, str + token.offset, token.len);
                    } else {
                        char_array_cat(key, " ");
                    }
                }

                char *key_str = char_array_get_string(key);
                log_debug("key_str=%s\n", key_str);
                address_expansion_array *expansions = address_dictionary_get_expansions(key_str);

                if (expansions != NULL) {
                    for (int j = 0; j < expansions->n; j++) {
                        address_expansion_t expansion = expansions->a[j];
                        if (expansion.canonical_index != NULL_CANONICAL_INDEX) {
                            char *canonical = address_dictionary_get_canonical(expansion.canonical_index);
                            if (phrase.start + phrase.len < tokens->n - 1) {
                                token_t next_token = tokens->a[phrase.start + phrase.len];
                                if (!is_numeric_token(next_token.type)) {
                                    string_tree_add_string(tree, canonical);
                                } else {
                                    uint32_t start_index = cstring_array_start_token(tree->strings);
                                    cstring_array_append_string(tree->strings, canonical);
                                    cstring_array_append_string(tree->strings, " ");
                                    cstring_array_terminate(tree->strings);
                                }
                            } else {
                                string_tree_add_string(tree, canonical);

                            }
                        } else {
                            for (int k = phrase.start; k < phrase.start + phrase.len; k++) {
                                token = tokens->a[k];
                                if (token.type != WHITESPACE) {
                                    string_tree_add_string_len(tree, str + token.offset, token.len);
                                } else {
                                    string_tree_add_string(tree, " ");
                                }
                            }

                        }
                    }

                    string_tree_finalize_token(tree);

                }
            } else {
                for (int j = phrase.start; j < phrase.start + phrase.len; j++) {
                    token = tokens->a[j];
                    if (token.type != WHITESPACE) {
                        log_debug("Adding previous token, %.*s\n", token.len, str + token.offset);
                        string_tree_add_string_len(tree, str + token.offset, token.len);
                    } else {
                        string_tree_add_string(tree, " ");
                    }
                    string_tree_finalize_token(tree);

                }
            }

            start = phrase.start + phrase.len;

        }

        char_array_destroy(key);

        end = (int)tokens->n;

        for (int j = start; j < end; j++) {
            token_t token = tokens->a[j]; 
            if (token.type != WHITESPACE) {
                log_debug("Adding previous token, %.*s\n", token.len, str + token.offset);

                string_tree_add_string_len(tree, str + token.offset, token.len);
            } else {
                log_debug("Adding space\n");
                string_tree_add_string(tree, " ");
            }
            string_tree_finalize_token(tree);       
        }


    } else {
        string_tree_add_string(tree, str);
        string_tree_finalize_token(tree);
    }

    if (phrases != NULL) {
        phrase_language_array_destroy(phrases);
    }

    token_array_destroy(tokens);

    return tree;
}

void add_postprocessed_string(cstring_array *strings, char *str, normalize_options_t options) {
    bool add_string = true;

    if (options.roman_numerals) {
        char *numex_replaced = replace_numeric_expressions(str, LATIN_LANGUAGE_CODE);
        if (numex_replaced != NULL) {
            cstring_array_add_string(strings, numex_replaced);
            free(numex_replaced);
            add_string = false;
        }
    }

    if (add_string) {
        cstring_array_add_string(strings, str);
    }
}

void expand_alternative(cstring_array *strings, khash_t(str_set) *unique_strings, char *str, normalize_options_t options) {
    size_t len = strlen(str);
    token_array *tokens = tokenize_keep_whitespace(str);
    string_tree_t *token_tree = string_tree_new_size(len);

    add_normalized_strings_token(token_tree, str, tokens, options);
    string_tree_iterator_t *tokenized_iter = string_tree_iterator_new(token_tree);

    string_tree_iterator_t *iter;

    char_array *temp_string = char_array_new_size(len);

    char *token;

    char *lang;

    kh_resize(str_set, unique_strings, kh_size(unique_strings) + tokenized_iter->remaining);

    for (; string_tree_iterator_done(tokenized_iter); string_tree_iterator_next(tokenized_iter)) {
        char_array_clear(temp_string);

        string_tree_iterator_foreach_token(tokenized_iter, token, {
            char_array_append(temp_string, token);
        })
        char_array_terminate(temp_string);

        char *tokenized_str = char_array_get_string(temp_string);

        char *new_str = tokenized_str;
        char *last_numex_str = NULL;
        if (options.expand_numex) {
            char *numex_replaced = NULL;
            for (int i = 0; i < options.num_languages; i++)  {
                lang = options.languages[i];

                numex_replaced = replace_numeric_expressions(new_str, lang);
                if (numex_replaced != NULL) {
                    new_str = numex_replaced;
                }

                if (last_numex_str != NULL) {
                    free(last_numex_str);
                }            
                last_numex_str = numex_replaced;
            }

        }
        
        string_tree_t *alternatives;

        khiter_t k = kh_get(str_set, unique_strings, new_str);

        if (k == kh_end(unique_strings)) {
            log_debug("Adding alternatives for single normalization\n");
            alternatives = add_string_alternatives(new_str, options);
            int ret;
            k = kh_put(str_set, unique_strings, strdup(new_str), &ret);           
            if (alternatives == NULL) {
                log_debug("alternatives = NULL\n");
            }
        } else {
            if (last_numex_str != NULL) {
                free(last_numex_str);
            }
            continue;
        }

        if (last_numex_str != NULL) {
            free(last_numex_str);
        }

        iter = string_tree_iterator_new(alternatives);
        log_debug("iter->num_tokens=%d", iter->num_tokens);

        for (; string_tree_iterator_done(iter); string_tree_iterator_next(iter)) {
            char_array_clear(temp_string);
            string_tree_iterator_foreach_token(iter, token, {
                log_debug("token=%s\n", token);
                char_array_append(temp_string, token);
            })
            char_array_terminate(temp_string);

            token = char_array_get_string(temp_string);
            add_postprocessed_string(strings, token, options);

        }

        string_tree_iterator_destroy(iter);
        string_tree_destroy(alternatives);
    }

    string_tree_iterator_destroy(tokenized_iter);
    string_tree_destroy(token_tree);

    token_array_destroy(tokens);

    char_array_destroy(temp_string);
}




cstring_array *expand_address(char *input, normalize_options_t options) {
    options.address_components |= ADDRESS_ANY;

    uint64_t normalize_string_options = 0;
    normalize_string_options |= options.transliterate ? NORMALIZE_STRING_TRANSLITERATE : 0;
    normalize_string_options |= options.latin_ascii ? NORMALIZE_STRING_LATIN_ASCII : 0;
    normalize_string_options |= options.decompose ? NORMALIZE_STRING_DECOMPOSE : 0;
    normalize_string_options |= options.strip_accents ? NORMALIZE_STRING_STRIP_ACCENTS : 0;
    normalize_string_options |= options.lowercase ? NORMALIZE_STRING_LOWERCASE : 0;
    normalize_string_options |= options.trim_string ? NORMALIZE_STRING_TRIM : 0;

    size_t len = strlen(input);

    string_tree_t *tree = normalize_string(input, normalize_string_options);

    cstring_array *strings = cstring_array_new_size(len);
    char_array *temp_string = char_array_new_size(len);

    khash_t(str_set) *unique_strings = kh_init(str_set);

    char *token;

    log_debug("string_tree_num_tokens(tree) = %d\n", string_tree_num_tokens(tree));

    if (string_tree_num_tokens(tree) == 1) {
        char *normalized = string_tree_get_alternative(tree, 0, 0);
        expand_alternative(strings, unique_strings, normalized, options);

    } else {
        log_debug("Adding alternatives for multiple normalizations\n");
        string_tree_iterator_t *iter = string_tree_iterator_new(tree);

        for (; string_tree_iterator_done(iter); string_tree_iterator_next(iter)) {
            char *segment;
            string_tree_iterator_foreach_token(iter, segment, {
                string_tree_t *subtree = add_string_alternatives(segment, options);
                string_tree_iterator_t *subiter = string_tree_iterator_new(subtree);
                for (; string_tree_iterator_done(subiter); string_tree_iterator_next(subiter)) {
                    char_array_clear(temp_string);

                    string_tree_iterator_foreach_token(subiter, token, {
                        log_debug("token=%s\n", token);
                        char_array_append(temp_string, token);
                    })
                    char_array_terminate(temp_string);

                    token = char_array_get_string(temp_string);
                    expand_alternative(strings, unique_strings, token, options);

                }

                string_tree_iterator_destroy(subiter);
                string_tree_destroy(subtree);
            })
        }

        string_tree_iterator_destroy(iter);
    }

    char *key_str = NULL;
    for (int i = kh_begin(unique_strings); i != kh_end(unique_strings); ++i) {
        if (!kh_exist(unique_strings, i)) continue;
        key_str = (char *)kh_key(unique_strings, i);
        free(key_str);
    }

    kh_destroy(str_set, unique_strings);

    char_array_destroy(temp_string);
    string_tree_destroy(tree);

    return strings;

}

bool libpostal_setup(void) {
    if (!transliteration_module_setup(DEFAULT_TRANSLITERATION_PATH)) {
        return false;
    }

    if (!numex_module_setup(DEFAULT_NUMEX_PATH)) {
        return false;
    }

    if (!address_dictionary_module_setup()) {
        return false;
    }

    return true;
}

void libpostal_teardown(void) {

    transliteration_module_teardown();

    numex_module_teardown();

    address_dictionary_module_teardown();
}
