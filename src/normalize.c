#include "normalize.h"

#define FULL_STOP_CODEPOINT 0x002e
#define APOSTROPHE_CODEPOINT 0x0027


char *utf8_normalize_string(char *str, uint64_t options) {    
    int utf8proc_options = UTF8PROC_OPTIONS_BASE | UTF8PROC_IGNORE | UTF8PROC_NLF2LF | UTF8PROC_STRIPCC;
    uint8_t *utf8proc_normalized = NULL;
    ssize_t normalized_len = 0;

    bool have_utf8proc_options = false;

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
        ssize_t normalized_len = utf8proc_map((uint8_t *)str, 0, &utf8proc_normalized, utf8proc_options);
        return (char *)utf8proc_normalized;
    }

    return NULL;
}

void add_latin_alternatives(string_tree_t *tree, char *str, size_t len, uint64_t options) {
    
    char *transliterated = NULL;
    char *utf8_normalized = NULL;
    char *prev_string = NULL;

    if (options & NORMALIZE_STRING_LATIN_ASCII) {
        transliterated = transliterate(LATIN_ASCII, str, len);
        if (transliterated != NULL) {
            utf8_normalized = utf8_normalize_string(transliterated, options);
            free(transliterated);
            transliterated = NULL;
        }

        if (utf8_normalized != NULL) {
            string_tree_add_string(tree, utf8_normalized);
            prev_string = utf8_normalized;
            utf8_normalized = NULL;
        }
    }

    utf8_normalized = utf8_normalize_string(str, options);

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
        char *ascii = NULL;

        if (options & NORMALIZE_STRING_LOWERCASE && is_ascii) {
            utf8_normalized = utf8_normalize_string(str, NORMALIZE_STRING_LOWERCASE);
            if (utf8_normalized != NULL) {
                string_tree_add_string(tree, utf8_normalized);
                free(utf8_normalized);
                utf8_normalized = NULL;
            }
            string_tree_finalize_token(tree);
        } else if (options & NORMALIZE_STRING_LATIN_ASCII && script == SCRIPT_LATIN && script_len > 0) {
            add_latin_alternatives(tree, str, script_len, options);
            string_tree_finalize_token(tree);
        } else if (options & NORMALIZE_STRING_TRANSLITERATE && script != SCRIPT_UNKNOWN && script_len > 0) {
            char *trans_name;
            foreach_transliterator(script, "", trans_name, {
                transliterated = transliterate(trans_name, str, script_len);

                if (transliterated != NULL) {
                    add_latin_alternatives(tree, transliterated, strlen(transliterated), options);
                    free(transliterated);
                }
            })

            string_tree_finalize_token(tree);
        } else {
            string_tree_add_string_len(tree, str, script_len);
        }
        consumed += script_len;
        str += script_len;
    }

    return tree;

}


void add_normalized_token(string_tree_t *tree, char *str, token_t token, uint64_t options) {
    size_t idx = 0;

    uint8_t *ptr = (uint8_t *)str + token.offset;
    size_t len = token.len;

    int32_t ch;
    ssize_t char_len;

    bool last_was_letter = false;
    bool append_char = true;

    cstring_array *array = tree->strings;

    size_t initial_n = array->str->n;

    while (idx < len) {
        char_len = utf8proc_iterate(ptr, len, &ch);
        
        if (char_len <= 0) break;

        bool is_hyphen = utf8_is_hyphen(ch);
        int cat = utf8proc_category(ch);

        bool is_letter = utf8_is_letter(cat);

        if (is_hyphen && last_was_letter && options & NORMALIZE_TOKEN_REPLACE_HYPHENS) {
            cstring_array_append_string(array, " ");
            append_char = false;
        } else if (is_hyphen && options & NORMALIZE_TOKEN_DELETE_HYPHENS) {
            append_char = false;
        }

        if (ch == FULL_STOP_CODEPOINT) {
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

            if (this_char == '\'' && next_char == 's') {
                break;
            } else if (this_char == 's' && next_char == '\'') {
                cstring_array_append_string(array, "s");
                break;
            }
        }

        if (ch == APOSTROPHE_CODEPOINT && options & NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE) {
            append_char = false;
        }

        if (append_char) {
            cstring_array_append_string_len(array, (char *)ptr, char_len);
        }

        ptr += char_len;
        idx += char_len;
        append_char = true;

        last_was_letter = is_letter;

    }

    if (array->str->n > initial_n) {
        string_tree_finalize_token(tree);
    }
}
