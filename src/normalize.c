#include "normalize.h"
#include "strndup.h"

#define FULL_STOP_CODEPOINT 0x002e
#define APOSTROPHE_CODEPOINT 0x0027

char *normalize_replace_numex(char *str, size_t num_languages, char **languages) {
    char *numex_normalized = NULL;

    for (size_t i = 0; i < num_languages; i++) {
        char *lang = languages[i];
        char *numex_replaced = replace_numeric_expressions(numex_normalized == NULL ? str : numex_normalized, lang);
        if (numex_replaced != NULL) {
            if (numex_normalized != NULL) {
                free(numex_normalized);
            }
            numex_normalized = numex_replaced;
        }
    }

    return numex_normalized;
}

char *normalize_string_utf8_languages(char *str, uint64_t options, size_t num_languages, char **languages) {
    int utf8proc_options = UTF8PROC_OPTIONS_BASE | UTF8PROC_IGNORE | UTF8PROC_NLF2LF | UTF8PROC_STRIPCC;
    uint8_t *utf8proc_normalized = NULL;

    bool have_utf8proc_options = false;

    char *normalized = NULL;
    bool normalized_allocated = false;

    if (options & NORMALIZE_STRING_TRIM) {
        char *trimmed = string_trim(str);
        if (trimmed != NULL) {
            normalized = trimmed;
            str = normalized;
            normalized_allocated = true;
        }
    }

    if (options & NORMALIZE_STRING_LOWERCASE) {
        char *lowercased = utf8_lower(str);
        if (lowercased != NULL) {
            if (normalized_allocated) {
                free(normalized);
            }
            normalized = lowercased;
            str = normalized;
            normalized_allocated = true;
        }
    }

    if (options & NORMALIZE_STRING_DECOMPOSE) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_NFD;
    }

    if (options & NORMALIZE_STRING_COMPOSE) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_NFC;
    }

    if (options & NORMALIZE_STRING_STRIP_ACCENTS) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_STRIP_ACCENTS;
    }

    if (have_utf8proc_options) {
        utf8proc_map((uint8_t *)str, 0, &utf8proc_normalized, utf8proc_options);

        if (utf8proc_normalized != NULL) {
            if (normalized_allocated) {
                free(normalized);
            }

            normalized = (char *)utf8proc_normalized;
            str = normalized;
            normalized_allocated = true;
        }
    }

    if (options & NORMALIZE_STRING_REPLACE_HYPHENS && string_contains_hyphen(str)) {
        char *replaced = string_replace_char(str, '-', ' ');
        if (replaced != NULL) {
            if (normalized_allocated) {
                free(normalized);
            }

            normalized = replaced;
            str = normalized;
            normalized_allocated = true;
        }
    }

    if (options & NORMALIZE_STRING_REPLACE_NUMEX && num_languages > 0) {
        char *numex_normalized = normalize_replace_numex(str, num_languages, languages);
        if (numex_normalized != NULL) {
            if (normalized_allocated) {
                free(normalized);
            }
            normalized = numex_normalized;
            str = normalized;
            normalized_allocated = true;
        }

    }

    return normalized;
}

char *normalize_string_utf8(char *str, uint64_t options) {
    return normalize_string_utf8_languages(str, options, 0, NULL);
}


char *normalize_string_latin_languages(char *str, size_t len, uint64_t options, size_t num_languages, char **languages) {
    char *transliterated = NULL;
    char *latin_transliterator = NULL;

    if (options & NORMALIZE_STRING_SIMPLE_LATIN_ASCII) {
        latin_transliterator = LATIN_ASCII_SIMPLE;
    } else if (options & NORMALIZE_STRING_LATIN_ASCII) {
        latin_transliterator = LATIN_ASCII;
    }

    if (latin_transliterator != NULL) {
        transliterated = transliterate(latin_transliterator, str, len);
    }

    char *utf8_normalized;
    if (transliterated == NULL) {
        utf8_normalized = normalize_string_utf8_languages(str, options, num_languages, languages);
    } else {
        utf8_normalized = normalize_string_utf8_languages(transliterated, options, num_languages, languages);
        free(transliterated);
        transliterated = NULL;
    }

    return utf8_normalized;
}

char *normalize_string_latin(char *str, size_t len, uint64_t options) {
    return normalize_string_latin_languages(str, len, options, 0, NULL);
}

void add_latin_alternatives(string_tree_t *tree, char *str, size_t len, uint64_t options, size_t num_languages, char **languages) {
    
    char *transliterated = NULL;
    char *utf8_normalized = NULL;
    char *prev_string = NULL;

    char *latin_transliterator = LATIN_ASCII;
    if (options & NORMALIZE_STRING_SIMPLE_LATIN_ASCII) {
        latin_transliterator = LATIN_ASCII_SIMPLE;
    }

    if (options & NORMALIZE_STRING_LATIN_ASCII) {
        transliterated = transliterate(latin_transliterator, str, len);
        if (transliterated != NULL) {
            utf8_normalized = normalize_string_utf8_languages(transliterated, options, num_languages, languages);
            free(transliterated);
            transliterated = NULL;
        }

        if (utf8_normalized != NULL) {
            string_tree_add_string(tree, utf8_normalized);
            prev_string = utf8_normalized;
            utf8_normalized = NULL;
        }
    }

    char *str_copy = strndup(str, len);
    utf8_normalized = normalize_string_utf8_languages(str_copy, options, num_languages, languages);
    free(str_copy);

    if (options & NORMALIZE_STRING_LATIN_ASCII && utf8_normalized != NULL) {
        transliterated = transliterate(latin_transliterator, utf8_normalized, strlen(utf8_normalized));
        free(utf8_normalized);
    } else {
        transliterated = utf8_normalized;
    }

    if (transliterated != NULL) {
        if (prev_string == NULL || strcmp(prev_string, transliterated) != 0) {
            string_tree_add_string(tree, transliterated);
        }
        free(transliterated);
        transliterated = NULL;
    } else {
        string_tree_add_string(tree, str);
    }

    if (prev_string != NULL) {
        free(prev_string);
    }
}


string_tree_t *normalize_string_languages(char *str, uint64_t options, size_t num_languages, char **languages) {
    size_t len = strlen(str);
    string_tree_t *tree = string_tree_new_size(len);

    size_t consumed = 0;

    khash_t(int_set) *scripts = kh_init(int_set);
    char *utf8_normalized = NULL;
    char *numex_replaced = NULL;

    script_t script;

    char *trans_name = NULL;
    char *lang;

    char *ptr = str;

    bool have_latin_transliterator = false;
    while (consumed < len)  {
        string_script_t script_span = get_string_script(ptr, len - consumed);
        script = script_span.script;
        size_t script_len = script_span.len;
        bool is_ascii = script_span.ascii;

        // Shortcut if the string is all ASCII
        if (options & NORMALIZE_STRING_LOWERCASE && is_ascii && script_len == len) {
            char *html_escaped = transliterate(HTML_ESCAPE, str, len);
            if (html_escaped != NULL) {
                str = html_escaped;
            }

            options ^= NORMALIZE_STRING_COMPOSE | NORMALIZE_STRING_DECOMPOSE | NORMALIZE_STRING_STRIP_ACCENTS | NORMALIZE_STRING_LATIN_ASCII;

            utf8_normalized = normalize_string_utf8_languages(str, options, num_languages, languages);
            if (utf8_normalized != NULL) {
                if (html_escaped != NULL) {
                    free(html_escaped);
                    html_escaped = NULL;
                }

                string_tree_add_string(tree, utf8_normalized);
                string_tree_finalize_token(tree);
                free(utf8_normalized);
                utf8_normalized = NULL;
            }

            kh_destroy(int_set, scripts);
            return tree;
        }

        log_debug("script_len=%zu\n", script_len);

        if (script == SCRIPT_LATIN && num_languages > 0 && !have_latin_transliterator) {
            for (size_t i = 0; i < num_languages; i++) {
                lang = languages[i];
                foreach_transliterator(script, lang, trans_name, {
                    if (!string_equals(trans_name, LATIN_ASCII)) {
                        have_latin_transliterator = true;
                        break;
                    }
                })

                if (have_latin_transliterator) break;
            }

        }

        if ((script != SCRIPT_LATIN || have_latin_transliterator) && script_len > 0) {
            int ret;
            khiter_t key = kh_put(int_set, scripts, (khint_t)script, &ret);
            if (ret < 0) {
                log_error("Error in kh_put\n");
                string_tree_destroy(tree);
                kh_destroy(int_set, scripts);
                return NULL;
            }
        }

        consumed += script_len;
        ptr += script_len;
    }

    if (!have_latin_transliterator) {
        add_latin_alternatives(tree, str, len, options, num_languages, languages);
    }

    size_t transliterate_scripts = kh_size(scripts);

    if (transliterate_scripts > 0) {
        string_tree_t *transliterators = string_tree_new_size(transliterate_scripts);

        khint_t key;

        kh_foreach_key(scripts, key, {
            script = (script_t)key;
            for (size_t i = 0; i < num_languages; i++) {
                lang = languages[i];
                foreach_transliterator(script, lang, trans_name, {
                    string_tree_add_string(transliterators, trans_name);
                })
            }

            foreach_transliterator(script, "", trans_name, {
                string_tree_add_string(transliterators, trans_name);
            })
            string_tree_finalize_token(transliterators);

        })


        string_tree_iterator_t *trans_iter = string_tree_iterator_new(transliterators);

        for (; !string_tree_iterator_done(trans_iter); string_tree_iterator_next(trans_iter)) {
            char *prev = NULL;
            char *transliterated = str;
            string_tree_iterator_foreach_token(trans_iter, trans_name, {
                log_debug("Doing %s\n", trans_name);
                transliterated = transliterate(trans_name, transliterated, strlen(transliterated));
                if (transliterated == NULL) {
                    transliterated = prev != NULL ? prev : str;
                    continue;
                }
                if (prev != NULL) {
                    free(prev);
                }
                prev = transliterated;
            })

            add_latin_alternatives(tree, transliterated, strlen(transliterated), options, num_languages, languages);
            if (transliterated != str) {
                free(transliterated);
            }
        }

        string_tree_iterator_destroy(trans_iter);
        string_tree_destroy(transliterators);

    }

    if (have_latin_transliterator) {
        add_latin_alternatives(tree, str, len, options, num_languages, languages);
    }
    
    kh_destroy(int_set, scripts);
    
    string_tree_finalize_token(tree);

    return tree;

}

inline string_tree_t *normalize_string(char *str, uint64_t options) {
    return normalize_string_languages(str, options, 0, NULL);
}

bool numeric_starts_with_alpha(char *str, token_t token) {
    if (token.type != NUMERIC || token.len == 0) return false;

    size_t idx = 0;

    uint8_t *ptr = (uint8_t *)str + token.offset;
    size_t len = token.len;

    int32_t ch;
    ssize_t char_len;

    bool contains_letter = false;
    bool append_char = true;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &ch);
        
        if (char_len <= 0) break;

        bool is_hyphen = utf8_is_hyphen(ch);
        int cat = utf8proc_category(ch);

        bool is_letter = utf8_is_letter(cat);
        bool is_number = utf8_is_number(cat);

        if (is_number) {
            return contains_letter;
        } else if (is_letter) {
            contains_letter = true;
        }

        ptr += char_len;
        idx += char_len;
    }

    return false;
}

void add_normalized_token(char_array *array, char *str, token_t token, uint64_t options) {
    size_t idx = 0;

    uint8_t *ptr = (uint8_t *)str + token.offset;
    size_t len = token.len;
    if (token.len > 0) {

        bool alpha_numeric_split = false;
        char *append_if_not_numeric = NULL;

        int32_t ch;
        int32_t next_ch;
        ssize_t char_len;
        ssize_t next_char_len;

        bool last_was_letter = false;
        bool last_was_number = false;
        bool append_char = true;

        while (idx < len) {
            char_len = utf8proc_iterate(ptr, len, &ch);

            if (char_len <= 0) break;

            bool is_hyphen = utf8_is_hyphen(ch);
            int cat = utf8proc_category(ch);

            bool is_letter = utf8_is_letter(cat);
            bool is_number = utf8_is_number(cat);

            next_char_len = utf8proc_iterate(ptr + char_len, len, &next_ch);
            int next_cat = utf8proc_category(next_ch);
            bool next_is_number = utf8_is_number(next_cat);
            bool next_is_letter = utf8_is_letter(next_cat);


            bool is_full_stop = ch == FULL_STOP_CODEPOINT;

            bool is_hyphen_between_letter_and_number = is_hyphen && ((next_is_number && last_was_letter) || (next_is_letter && last_was_number));

            if (is_hyphen && options & NORMALIZE_TOKEN_REPLACE_HYPHENS && (!(last_was_number && next_is_number) || options & NORMALIZE_TOKEN_REPLACE_NUMERIC_HYPHENS)) {
                char_array_append(array, " ");
                append_char = false;
            } else if (is_hyphen && options & NORMALIZE_TOKEN_DELETE_HYPHENS) {
                append_char = is_hyphen_between_letter_and_number;
            }

            if ((is_hyphen || is_full_stop) && token.type == NUMERIC && options & NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC && last_was_letter) {
                ptr += char_len;
                idx += char_len;
                append_if_not_numeric = is_hyphen ? "-" : ".";
                append_char = true;
                continue;
            }

            if (!is_number && append_if_not_numeric != NULL) {
                char_array_append(array, append_if_not_numeric);
                append_if_not_numeric = NULL;
            }

            if (is_number && options & NORMALIZE_TOKEN_REPLACE_DIGITS) {
                char_array_append(array, DIGIT_CHAR);
                append_char = false;
            }

            if (options & NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC && token.type == NUMERIC && ((last_was_letter && is_number) || (last_was_number && is_letter)) && !alpha_numeric_split) {
                char_array_append(array, " ");
                alpha_numeric_split = true;
            }

            if (is_full_stop) {
                if (options & NORMALIZE_TOKEN_DELETE_FINAL_PERIOD && idx == len - 1) {
                    break;
                }

                if (token.type == ACRONYM && options & NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS) {
                    append_char = false;
                }
            }

            if (idx == len - 2 && len > 2 && options & NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES) {
                char this_char = *ptr;
                char next_char = *(ptr + 1);

                if ((this_char == '\'' && next_char == 's') || (this_char == 's' && next_char == '\'')) {
                    char_array_append(array, "s");
                    break;
                }
            }

            if (ch == APOSTROPHE_CODEPOINT && token.type == WORD && options & NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE) {
                append_char = false;
            }

            if (append_char) {
                char_array_append_len(array, (char *)ptr, char_len);
            }

            ptr += char_len;
            idx += char_len;
            append_char = true;

            last_was_letter = is_letter;
            last_was_number = is_number;
        }
    }

    char_array_terminate(array);

}

inline void normalize_token(cstring_array *array, char *str, token_t token, uint64_t options) {
    cstring_array_start_token(array);
    add_normalized_token(array->str, str, token, options);
}
