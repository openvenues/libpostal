#include "normalize.h"

#define FULL_STOP_CODEPOINT 0x002e
#define APOSTROPHE_CODEPOINT 0x0027


char *normalize_string_utf8(char *str, uint64_t options) {    
    int utf8proc_options = UTF8PROC_OPTIONS_BASE | UTF8PROC_IGNORE | UTF8PROC_NLF2LF | UTF8PROC_STRIPCC;
    uint8_t *utf8proc_normalized = NULL;

    bool have_utf8proc_options = false;

    if (options & NORMALIZE_STRING_TRIM) {
        string_trim(str);
    }

    if (options & NORMALIZE_STRING_DECOMPOSE) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_NFD;
    }

    if (options & NORMALIZE_STRING_STRIP_ACCENTS) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_STRIP_ACCENTS;
    }

    if (options & NORMALIZE_STRING_LOWERCASE) {
        have_utf8proc_options = true;
        utf8proc_options |= UTF8PROC_OPTIONS_LOWERCASE;
    }

    if (have_utf8proc_options) {
        utf8proc_map((uint8_t *)str, 0, &utf8proc_normalized, utf8proc_options);
        return (char *)utf8proc_normalized;
    }

    return NULL;
}


char *normalize_string_latin(char *str, size_t len, uint64_t options) {
    char *transliterated = transliterate(LATIN_ASCII, str, len);
    
    char *utf8_normalized;
    if (transliterated == NULL) {
        utf8_normalized = normalize_string_utf8(str, options);
    } else {
        utf8_normalized = normalize_string_utf8(transliterated, options);
        free(transliterated);
        transliterated = NULL;
    }

    return utf8_normalized;
}

void add_latin_alternatives(string_tree_t *tree, char *str, size_t len, uint64_t options) {
    
    char *transliterated = NULL;
    char *utf8_normalized = NULL;
    char *prev_string = NULL;

    if (options & NORMALIZE_STRING_LATIN_ASCII) {
        transliterated = transliterate(LATIN_ASCII, str, len);
        if (transliterated != NULL) {
            utf8_normalized = normalize_string_utf8(transliterated, options);
            free(transliterated);
            transliterated = NULL;
        }

        if (utf8_normalized != NULL) {
            string_tree_add_string(tree, utf8_normalized);
            prev_string = utf8_normalized;
            utf8_normalized = NULL;
        }
    }

    utf8_normalized = normalize_string_utf8(str, options);

    if (options & NORMALIZE_STRING_LATIN_ASCII && utf8_normalized != NULL) {
        transliterated = transliterate(LATIN_ASCII, utf8_normalized, strlen(utf8_normalized));
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
    }

    if (prev_string != NULL) {
        free(prev_string);
    }

}

string_tree_t *normalize_string(char *str, uint64_t options) {
    size_t len = strlen(str);
    string_tree_t *tree = string_tree_new_size(len);

    size_t consumed = 0;

    while (consumed < len)  {            

        string_script_t script_span = get_string_script(str, len - consumed);
        script_t script = script_span.script;
        size_t script_len = script_span.len;
        bool is_ascii = script_span.ascii;

        char *utf8_normalized = NULL;
        char *transliterated = NULL;

        if (options & NORMALIZE_STRING_LOWERCASE && is_ascii) {
            utf8_normalized = normalize_string_utf8(str, NORMALIZE_STRING_LOWERCASE);
            if (utf8_normalized != NULL) {

                if (options & NORMALIZE_STRING_LATIN_ASCII) {
                    transliterated = transliterate(LATIN_ASCII, utf8_normalized, len);
                    if (transliterated != NULL) {
                        string_tree_add_string(tree, transliterated);
                        free(transliterated);
                        transliterated = NULL;
                    }
                } else {
                    string_tree_add_string(tree, utf8_normalized);
                }
                free(utf8_normalized);
                utf8_normalized = NULL;

            }
        } else if (options & NORMALIZE_STRING_LATIN_ASCII && script == SCRIPT_LATIN && script_len > 0) {
            add_latin_alternatives(tree, str, script_len, options);
        } else if (options & NORMALIZE_STRING_TRANSLITERATE && script != SCRIPT_UNKNOWN && script_len > 0) {
            char *trans_name;
            char *original = strndup(str, script_len);
            if (original != NULL) {
                add_latin_alternatives(tree, original, script_len, options);
                free(original);
            }

            foreach_transliterator(script, "", trans_name, {
                transliterated = transliterate(trans_name, str, script_len);

                if (transliterated != NULL) {
                    add_latin_alternatives(tree, transliterated, strlen(transliterated), options);
                    free(transliterated);
                }
            })

        } else {
            string_tree_add_string_len(tree, str, script_len);
        }
        string_tree_finalize_token(tree);

        consumed += script_len;
        str += script_len;
    }


    return tree;

}

void add_normalized_token(char_array *array, char *str, token_t token, uint64_t options) {
    size_t idx = 0;

    uint8_t *ptr = (uint8_t *)str + token.offset;
    size_t len = token.len;
    if (token.len == 0) return;

    bool alpha_numeric_split = false;
    char *append_if_not_numeric = NULL;

    int32_t ch;
    ssize_t char_len;

    bool last_was_letter = false;
    bool append_char = true;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &ch);
        
        if (char_len <= 0) break;

        bool is_hyphen = utf8_is_hyphen(ch);
        int cat = utf8proc_category(ch);

        bool is_letter = utf8_is_letter(cat);
        bool is_number = utf8_is_number(cat);

        bool is_full_stop = ch == FULL_STOP_CODEPOINT;

        if (is_hyphen && last_was_letter && options & NORMALIZE_TOKEN_REPLACE_HYPHENS) {
            char_array_append(array, " ");
            append_char = false;
        } else if (is_hyphen && options & NORMALIZE_TOKEN_DELETE_HYPHENS) {
            append_char = false;
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

        if (options & NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC && token.type == NUMERIC && last_was_letter && is_number && !alpha_numeric_split) {
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

    }

    char_array_terminate(array);

}

inline void normalize_token(cstring_array *array, char *str, token_t token, uint64_t options) {
    cstring_array_start_token(array);
    add_normalized_token(array->str, str, token, options);
}
