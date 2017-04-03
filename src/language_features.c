#include "language_features.h"
#include "language_classifier.h"
#include "address_dictionary.h"
#include "features.h"
#include "normalize.h"
#include "scanner.h"
#include "unicode_scripts.h"

#define UNIGRAMS 1
#define BIGRAMS 2
#define QUADGRAMS 4
#define OCTAGRAMS 8

#define LANGUAGE_CLASSIFIER_NORMALIZE_STRING_OPTIONS NORMALIZE_STRING_LOWERCASE | NORMALIZE_STRING_LATIN_ASCII | NORMALIZE_STRING_REPLACE_HYPHENS
#define LANGUAGE_CLASSIFIER_NORMALIZE_TOKEN_OPTIONS NORMALIZE_TOKEN_DELETE_FINAL_PERIOD | NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS | NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC

inline char *language_classifier_normalize_string(char *str) {
    return normalize_string_latin(str, strlen(str), LANGUAGE_CLASSIFIER_NORMALIZE_STRING_OPTIONS);
}


inline void language_classifier_normalize_token(char_array *array, char *str, token_t token) {
    char_array_strip_nul_byte(array);
    if (is_word_token(token.type)) {
        add_normalized_token(array, str, token, LANGUAGE_CLASSIFIER_NORMALIZE_TOKEN_OPTIONS);
    } else {
        char_array_add(array, " ");
    }
}


static inline void append_prefix(char_array *array, char *prefix) {
    if (prefix != NULL) {
        char_array_append(array, prefix);
        char_array_append(array, NAMESPACE_SEPARATOR_CHAR);
    }
}

static inline void add_full_token_feature(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, token_t token) {
    if (features == NULL || feature_array == NULL) return;

    char_array_clear(feature_array);
    append_prefix(feature_array, prefix);

    char_array_add_len(feature_array, str + token.offset, token.len);

    if (feature_array->n <= 1) return;
    char *feature = char_array_get_string(feature_array);
    log_debug("full token feature=%s\n", feature);
    feature_counts_add(features, feature, 1.0);
}


static void add_ngram_features(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, token_t token, size_t n) {
    char *feature_namespace;
    if (features == NULL || feature_array == NULL) return;

    if (n == 0 || !is_word_token(token.type)) return;
    
    size_t lengths[n];
    size_t num_chars = 0;

    size_t offset = token.offset;

    uint8_t *ptr = (uint8_t *)str + offset;

    int32_t ch;

    size_t idx = 0;
    size_t len = token.len;

    size_t gram_len = 0;
    size_t gram_offset = 0;
    size_t consumed = 0;

    bool beginning = true;

    log_debug("len = %zu\n", len);

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);
        if (char_len <= 0 || ch == 0) break;

        // Not at min characters yet
        if (num_chars < n) {
            lengths[num_chars] = (size_t)char_len;
            num_chars++;
            gram_len += char_len;
        }

        // We have a full gram of size n
        if (num_chars == n) {
            char_array_clear(feature_array);
            append_prefix(feature_array, prefix);

            if (beginning) {
                beginning = false;
            } else {
                char_array_append(feature_array, "_");
                gram_len -= lengths[0];
                gram_offset += lengths[0];
                gram_len += char_len;

                for (size_t i = 1; i < n; i++) {
                    lengths[i - 1] = lengths[i];
                }
                lengths[n - 1] = (size_t)char_len;
            }

            char_array_append_len(feature_array, str + offset + gram_offset, gram_len);

            if (idx + char_len < len) {
                char_array_append(feature_array, "_");
            }

            char_array_terminate(feature_array);
            if (feature_array->n <= 1) continue;
            char *feature = char_array_get_string(feature_array);
            log_debug("feature=%s\n", feature);

            feature_counts_add(features, feature, 1.0);
        }

        idx += char_len;
        ptr += char_len;
        consumed += char_len;
    }

    if (num_chars < n) {
        add_full_token_feature(features, prefix, feature_array, str, token);
    }

}

static void add_phrase_feature(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, phrase_t phrase, token_array *tokens) {
    if (features == NULL || feature_array == NULL || tokens == NULL || tokens->n == 0) return;
    char_array_clear(feature_array);
    append_prefix(feature_array, prefix);

    token_t token;
    for (size_t i = phrase.start; i < phrase.start + phrase.len; i++) {
        token = tokens->a[i];
        char_array_append_len(feature_array, str + token.offset, token.len);
        if (i < phrase.start + phrase.len - 1 && !is_ideographic(token.type)) {
            char_array_append(feature_array, " ");
        }
    }

    char_array_terminate(feature_array);
    if (feature_array->n <= 1) return;
    char *feature = char_array_get_string(feature_array);
    feature_counts_add(features, feature, 1.0);
}


static void add_prefix_phrase_feature(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, phrase_t phrase, token_t token) {
    if (features == NULL || feature_array == NULL || phrase.len == 0 || phrase.len >= token.len) return;
    char_array_clear(feature_array);

    append_prefix(feature_array, prefix);

    char_array_append(feature_array, "pfx=");

    char_array_add_len(feature_array, str + token.offset, phrase.len);

    if (feature_array->n <= 1) return;
    char *feature = char_array_get_string(feature_array);
    feature_counts_add(features, feature, 1.0);

}


static void add_suffix_phrase_feature(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, phrase_t phrase, token_t token) {
    if (features == NULL || feature_array == NULL || phrase.len == 0 || phrase.len >= token.len) return;
    char_array_clear(feature_array);

    append_prefix(feature_array, prefix);

    char_array_append(feature_array, "sfx=");

    char_array_add_len(feature_array, str + token.offset + token.len - phrase.len, phrase.len);

    if (feature_array->n <= 1) return;
    char *feature = char_array_get_string(feature_array);
    feature_counts_add(features, feature, 1.0);

}


static void add_token_features(khash_t(str_double) *features, char *prefix, char_array *feature_array, char *str, token_t token) {
    // Non-words don't convey any language information
    // TODO: ordinal number suffixes may be worth investigating
    if (!is_word_token(token.type)) {
        return;
    }

    phrase_t prefix_phrase = search_address_dictionaries_prefix(str + token.offset, token.len, NULL);
    if (prefix_phrase.len > 0 && prefix_phrase.len < token.len) {
        add_prefix_phrase_feature(features, prefix, feature_array, str, prefix_phrase, token);
    }

    phrase_t suffix_phrase = search_address_dictionaries_suffix(str + token.offset, token.len, NULL);
    if (suffix_phrase.len > 0 && suffix_phrase.len < token.len) {
        add_suffix_phrase_feature(features, prefix, feature_array, str, suffix_phrase, token);
    }

    if (!is_ideographic(token.type)) {
        // Add quadgram features
        add_ngram_features(features, prefix, feature_array, str, token, QUADGRAMS);
    } else {
        // For ideographic scripts, use single ideograms
        add_full_token_feature(features, prefix, feature_array, str, token);
    }
}

static void add_script_feature(khash_t(str_double) *features, char *prefix, char_array *feature_array, script_t script) {
    char_array_clear(feature_array);
    char_array_append(feature_array, "sc=");
    char_array_cat_printf(feature_array, "%d", script);
    char *feature = char_array_get_string(feature_array);
    feature_counts_add(features, feature, 1.0);
}


khash_t(str_double) *extract_language_features(char *str, char *country, token_array *tokens, char_array *feature_array) {
    if (str == NULL || tokens == NULL || feature_array == NULL) return NULL;

    char *feature;

    char *prefix = country;

    size_t consumed = 0;
    size_t len = strlen(str);
    if (len == 0) return NULL;

    char_array *normalized = char_array_new_size(len);
    if (normalized == NULL) {
        return NULL;
    }

    khash_t(str_double) *features = kh_init(str_double);
    if (features == NULL) {
        char_array_destroy(normalized);
        return NULL;
    }

    while (consumed < len) {
        string_script_t str_script = get_string_script(str, len - consumed);
        log_debug("str=%s, len=%zu, consumed=%zu, script_len=%zu\n", str, strlen(str), consumed, str_script.len);

        script_languages_t script_langs = get_script_languages(str_script.script);

        if (script_langs.num_languages > 1) {
            token_array_clear(tokens);
            bool keep_whitespace = true;
            tokenize_add_tokens(tokens, (const char *)str, str_script.len, keep_whitespace);

            size_t num_tokens = tokens->n;
            token_t token;

            char_array_clear(normalized);

            for (size_t i = 0; i < num_tokens; i++) {
                token = tokens->a[i];
                language_classifier_normalize_token(normalized, str, token);
            }
            char_array_terminate(normalized);

            char *normalized_str = char_array_get_string(normalized);
            token_array_clear(tokens);
            keep_whitespace = false;
            tokenize_add_tokens(tokens, (const char *)normalized_str, strlen(normalized_str), keep_whitespace);

            token_t prev_token;
            char *phrase = NULL;

            // Search address dictionaries for any language
            phrase_array *phrases = search_address_dictionaries_tokens(normalized_str, tokens, NULL);
            log_debug("normalized_str=%s\n", normalized_str);

            size_t i, j;

            if (phrases != NULL) {
                for (i = 0; i < phrases->n; i++) {
                    phrase_t phrase = phrases->a[i];
                    log_debug("phrase (%d, %d)\n", phrase.start, phrase.len);
                    add_phrase_feature(features, prefix, feature_array, normalized_str, phrase, tokens);
                }

                phrase_array_destroy(phrases);
            }

            for (j = 0; j < tokens->n; j++) {
                token = tokens->a[j];
                
                add_token_features(features, prefix, feature_array, normalized_str, token);
            }

            if (str_script.script != SCRIPT_LATIN) {
                add_script_feature(features, prefix, feature_array, str_script.script);
                log_debug("script feature=%s\n", feature);                
            }

        } else if (str_script.script != SCRIPT_UNKNOWN && str_script.script != SCRIPT_COMMON && script_langs.num_languages > 0) {
            add_script_feature(features, prefix, feature_array, str_script.script);
        }

        consumed += str_script.len;
        str += str_script.len;
    }

    char_array_destroy(normalized);

    return features;
}
