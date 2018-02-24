#include <stdio.h>
#include "log/log.h"
#include "string_utils.h"
#include "strndup.h"

#define INVALID_INDEX(i, n) ((i) < 0 || (i) >= (n))

int string_compare_case_insensitive(const char *str1, const char *str2) {
    int c1, c2;

    do {
        c1 =  tolower(*str1++);
        c2 = tolower(*str2++);
    } while (c1 == c2 && c1 != 0);

    return c1 - c2;
}

int string_compare_len_case_insensitive(const char *str1, const char *str2, size_t len) {
    register unsigned char *s1 = (unsigned char *) str1;
    register unsigned char *s2 = (unsigned char *) str2;

    unsigned char c1, c2;

    if (len == 0) return 0;

    do {
        c1 = *s1++;
        c2 = *s2++;
        if (!c1 || !c2)
            break;
        if (c1 == c2)
            continue;
        c1 = tolower(c1);
        c2 = tolower(c2);
        if (c1 != c2)
            break;
    } while (--len);

    return (int)c1 - (int)c2;

}

inline size_t string_common_prefix(const char *str1, const char *str2) {
    size_t common_prefix;
    for (common_prefix = 0; *str1 && *str2 && *str1 == *str2; str1++, str2++)
        common_prefix++;
    return common_prefix;
}

inline size_t string_common_suffix(const char *str1, const char *str2) {
    size_t common_suffix = 0;
    size_t str1_len = strlen(str1);
    size_t str2_len = strlen(str2);
    size_t min_len = (str1_len < str2_len) ? str1_len : str2_len;
    for (int i=1; i <= min_len && str1[str1_len-i] == str2[str2_len-i]; i++)
        common_suffix++;
    return common_suffix;
}

inline bool string_starts_with(const char *str, const char *start) {
    for (; *start; str++, start++)
        if (*str != *start)
            return false;
    return true;
}

inline bool string_ends_with(const char *str, const char *ending) {
    size_t end_len = strlen(ending);
    size_t str_len = strlen(str);

    return str_len < end_len ? false : !strcmp(str + str_len - end_len, ending);
}

inline bool string_equals(const char *s1, const char *s2) {
    if (s1 == NULL || s2  == NULL) return false;
    return strcmp(s1, s2) == 0;
}

inline void string_upper(char *s) {
    for (; *s; ++s) *s = toupper(*s);
}

inline char *string_replace_char(char *str, char c1, char c2) {
    char *repl = strdup(str);
    if (repl == NULL) return NULL;
    char *ptr = repl;
    for (; *ptr; ++ptr) {
        if (*ptr == c1) *ptr = c2;
    }
    return repl;
}

bool string_replace_with_array(char *str, char *replace, char *with, char_array *result) {
    if (str == NULL) return false;
    if (result == NULL) return false;

    if (replace == NULL) {
        replace = "";
    }

    size_t len_replace = strlen(replace);
    if (len_replace == 0) {
        return true;        
    }

    if (with == NULL) {
        with = "";
    }

    size_t len_with = strlen(with);

    char *temp;
    char *start = str;

    for (size_t count = 0; (temp = strstr(start, replace)); count++) {
        char_array_cat_len(result, start, temp - start);
        char_array_cat_len(result, with, len_with);
        start = temp + len_replace;
    }

    char_array_cat(result, start);

    return true;
}

char *string_replace(char *str, char *replace, char *with) {
    if (str == NULL) return NULL;

    char_array *array = char_array_new_size(strlen(str));
    if (!string_replace_with_array(str, replace, with, array)) {
        char_array_destroy(array);
        return NULL;
    }
    return char_array_to_string(array);
}


inline bool string_is_upper(char *s) {
    for (; *s; ++s) {
        if (*s != toupper(*s)) return false;
    }
    return true;
}

inline void string_lower(char *s) {
    for (; *s; ++s) *s = tolower(*s);
}

inline bool string_is_lower(char *s) {
    for (; *s; ++s) {
        if (*s != tolower(*s)) return false;
    }
    return true;
}

uint32_t string_translate(char *str, size_t len, char *word_chars, char *word_repls, size_t trans_len) {
    uint32_t num_replacements = 0;
    
    for (int i = 0; i < len; i++) {
        for (int j = 0; j < trans_len; j++) {
            if (str[i] == word_chars[j]) {
                str[i] = word_repls[j];
                num_replacements++;
                break;
            }
        }
    }
    return num_replacements;
}

ssize_t utf8proc_iterate_reversed(const uint8_t *str, ssize_t start, int32_t *dst) {
    ssize_t len = 0;

    const uint8_t *ptr = str + start;

    *dst = -1;

    do {
        if (ptr <= str) return 0;
        ptr--; len++;
    } while ((*ptr & 0xC0) == 0x80);

    int32_t ch = 0;

    ssize_t ret_len = utf8proc_iterate(ptr, len, &ch);
    *dst = ch;
    return ret_len;
}

char *utf8_reversed_string(const char *s) {
    int32_t unich;
    ssize_t len, remaining;

    size_t slen = strlen(s);

    char *out = malloc(slen + 1);

    uint8_t *ptr =  (uint8_t *)s;
    char *rev_ptr = out + slen;

    while(1) {
        len = utf8proc_iterate(ptr, -1, &unich);
        remaining = len;
        if (len <= 0 || unich == 0) break;
        if (!(utf8proc_codepoint_valid(unich))) goto error_free_output;

        rev_ptr -= len;
        memcpy(rev_ptr, (char *)ptr, len);

        ptr += len;

    }

    out[slen] = '\0';
    return out;

error_free_output:
    free(out);
    return NULL;
}

typedef enum casing_option {
    UTF8_LOWER,
    UTF8_UPPER
} casing_option_t;

char *utf8_case(const char *s, casing_option_t casing, utf8proc_option_t options) {
    ssize_t len = (ssize_t)strlen(s);
    utf8proc_uint8_t *str = (utf8proc_uint8_t *)s;

    utf8proc_ssize_t result;
    result = utf8proc_decompose(str, len, NULL, 0, options);

    if (result < 0) return NULL;
    utf8proc_int32_t *buffer = (utf8proc_int32_t *) malloc(result * sizeof(utf8proc_int32_t) + 1);
    if (buffer == NULL) return NULL;

    result = utf8proc_decompose(str, len, buffer, result, options);
    if (result < 0) {
        free(buffer);
        return NULL;
    }

    for (utf8proc_ssize_t i = 0; i < result; i++) {
        utf8proc_int32_t uc = buffer[i];
        utf8proc_int32_t norm = uc;

        if (casing == UTF8_LOWER) {
            norm = utf8proc_tolower(uc);
        } else if (casing == UTF8_UPPER) {
            norm = utf8proc_toupper(uc);
        }
        buffer[i] = norm;
    }

    result = utf8proc_reencode(buffer, result, options);
    if (result < 0) {
        free(buffer);
        return NULL;
    }

    utf8proc_int32_t *newptr = (utf8proc_int32_t *) realloc(buffer, (size_t)result+1);
    if (newptr != NULL) {
        buffer = newptr;
    } else {
        free(buffer);
        return NULL;
    }

    return (char *)buffer;
}

inline char *utf8_lower_options(const char *s, utf8proc_option_t options) {
    return utf8_case(s, UTF8_LOWER, options);
}

inline char *utf8_lower(const char *s) {
    return utf8_case(s, UTF8_LOWER, UTF8PROC_OPTIONS_NFC);
}

inline char *utf8_upper_options(const char *s, utf8proc_option_t options) {
    return utf8_case(s, UTF8_UPPER, options);
}

inline char *utf8_upper(const char *s) {
    return utf8_case(s, UTF8_UPPER, UTF8PROC_OPTIONS_NFC);
}


inline bool utf8_is_letter(int cat) {
    return cat == UTF8PROC_CATEGORY_LL || cat == UTF8PROC_CATEGORY_LU        \
            || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LO    \
            || cat == UTF8PROC_CATEGORY_LM;
}

inline bool utf8_is_digit(int cat) {
    return cat == UTF8PROC_CATEGORY_ND;
}

inline bool utf8_is_number(int cat) {
    return cat == UTF8PROC_CATEGORY_ND || cat == UTF8PROC_CATEGORY_NL || cat == UTF8PROC_CATEGORY_NO;
}

inline bool utf8_is_letter_or_number(int cat) {
    return cat == UTF8PROC_CATEGORY_LL || cat == UTF8PROC_CATEGORY_LU        \
            || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LO    \
            || cat == UTF8PROC_CATEGORY_LM || cat == UTF8PROC_CATEGORY_ND    \
            || cat == UTF8PROC_CATEGORY_NL || cat == UTF8PROC_CATEGORY_NO;
}

inline bool utf8_is_hyphen(int32_t ch) {
    int cat = utf8proc_category(ch);
    return cat == UTF8PROC_CATEGORY_PD || ch == 0x2212;
}

#define PERIOD_CODEPOINT 46

inline bool utf8_is_period(int32_t codepoint) {
    return codepoint == PERIOD_CODEPOINT;
}

inline bool utf8_is_punctuation(int cat) {
    return cat == UTF8PROC_CATEGORY_PD || cat == UTF8PROC_CATEGORY_PE        \
           || cat == UTF8PROC_CATEGORY_PF || cat == UTF8PROC_CATEGORY_PI    \
           || cat == UTF8PROC_CATEGORY_PO || cat == UTF8PROC_CATEGORY_PS;
}

inline bool utf8_is_symbol(int cat) {
    return cat == UTF8PROC_CATEGORY_SK || cat == UTF8PROC_CATEGORY_SC       \
           || cat == UTF8PROC_CATEGORY_SM || cat == UTF8PROC_CATEGORY_SO;
}

inline bool utf8_is_separator(int cat) {
    return cat == UTF8PROC_CATEGORY_ZS || cat == UTF8PROC_CATEGORY_ZL || cat == UTF8PROC_CATEGORY_ZP;
}

inline bool utf8_is_whitespace(int32_t ch) {
    int cat = utf8proc_category(ch);
    return utf8_is_separator(cat) || 
           ch == 9 || // character tabulation
           ch == 10 || // line feed
           ch == 11 || // line tabulation
           ch == 12 || // form feed
           ch == 13 || // carriage return
           ch == 133 // next line
           ;
}


ssize_t utf8_len(const char *str, size_t len) {
    if (str == NULL) return -1;
    if (len == 0) return 0;

    int32_t ch = 0;
    ssize_t num_utf8_chars = 0;
    ssize_t char_len;

    uint8_t *ptr = (uint8_t *)str;

    size_t remaining = len;

    while (1) {
        char_len = utf8proc_iterate(ptr, -1, &ch);

        if (ch == 0) break;
        remaining -= char_len;
        if (remaining == 0) break;

        ptr += char_len;
        num_utf8_chars++;
    }

    return num_utf8_chars;
}

uint32_array *unicode_codepoints(const char *str) {
    if (str == NULL) return NULL;

    uint32_array *a = uint32_array_new();

    int32_t ch = 0;
    ssize_t num_utf8_chars = 0;
    ssize_t char_len;

    uint8_t *ptr = (uint8_t *)str;

    while (1) {
        char_len = utf8proc_iterate(ptr, -1, &ch);

        if (ch == 0) break;

        uint32_array_push(a, (uint32_t)ch);
        ptr += char_len;
    }

    return a;
}

bool unicode_equals(uint32_array *u1_array, uint32_array *u2_array) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;
    if (len1 != len2) return false;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;
    for (size_t i = 0; i < len1; i++) {
        if (u1[i] != u2[i]) return false;
    }
    return true;
}

size_t unicode_common_prefix(uint32_array *u1_array, uint32_array *u2_array) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    size_t min_len = len1 <= len2 ? len1 : len2;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;
    size_t common_prefix = 0;

    for (size_t i = 0; i < min_len; i++) {
        if (u1[i] == u2[i]) {
            common_prefix++;
        } else {
            break;
        }
    }
    return common_prefix;
}

size_t unicode_common_suffix(uint32_array *u1_array, uint32_array *u2_array) {
    size_t len1 = u1_array->n;
    size_t len2 = u2_array->n;

    size_t min_len = len1 <= len2 ? len1 : len2;

    uint32_t *u1 = u1_array->a;
    uint32_t *u2 = u2_array->a;
    size_t common_suffix = 0;

    for (size_t i = 0; i < min_len; i++) {
        if (u1[len1 - i - 1] == u2[len2 - i - 1]) {
            common_suffix++;
        } else {
            break;
        }
    }
    return common_suffix;
}




int utf8_compare_len_option(const char *str1, const char *str2, size_t len, bool case_insensitive) {
    if (len == 0) return 0;

    int32_t c1, c2;
    ssize_t len1, len2;

    uint8_t *ptr1 = (uint8_t *)str1;
    uint8_t *ptr2 = (uint8_t *)str2;

    size_t remaining = len;

    while (1) {
        len1 = utf8proc_iterate(ptr1, -1, &c1);
        len2 = utf8proc_iterate(ptr2, -1, &c2);

        if (c1 == 0 || c2 == 0) break;

        if (c1 == c2 || (case_insensitive && utf8proc_tolower(c1) == utf8proc_tolower(c2))) {
            ptr1 += len1;
            ptr2 += len2;
            remaining -= len1;
        } else {
            break;
        }

        if (remaining == 0) break;

    }

    return (int) c1 - c2;
}

inline int utf8_compare_len(const char *str1, const char *str2, size_t len) {
    return utf8_compare_len_option(str1, str2, len, false);
}

inline int utf8_compare(const char *str1, const char *str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t max_len = len1 >= len2 ? len1 : len2;
    return utf8_compare_len_option(str1, str2, max_len, false);
}

inline int utf8_compare_len_case_insensitive(const char *str1, const char *str2, size_t len) {
    return utf8_compare_len_option(str1, str2, len, true);
}

inline int utf8_compare_case_insensitive(const char *str1, const char *str2, size_t len) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t max_len = len1 >= len2 ? len1 : len2;
    return utf8_compare_len_option(str1, str2, max_len, true);
}


size_t utf8_common_prefix_len(const char *str1, const char *str2, size_t len) {
    size_t common_prefix = 0;

    if (len == 0) return common_prefix;

    int32_t c1 = 0;
    int32_t c2 = 0;

    size_t remaining = len;

    ssize_t len1, len2;

    uint8_t *ptr1 = (uint8_t *)str1;
    uint8_t *ptr2 = (uint8_t *)str2;

    while (1) {
        len1 = utf8proc_iterate(ptr1, -1, &c1);
        len2 = utf8proc_iterate(ptr2, -1, &c2);

        if (c1 <= 0 || c2 <= 0) break;
        if (c1 == c2) {
            ptr1 += len1;
            ptr2 += len2;
            common_prefix += len1;
            if (common_prefix >= len) {
                return common_prefix;
            }
        } else {
            break;
        }
    }

    return common_prefix;
}

size_t utf8_common_prefix(const char *str1, const char *str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    size_t len = len1 <= len2 ? len1 : len2;

    return utf8_common_prefix_len(str1, str2, len);
}


size_t utf8_common_prefix_len_ignore_separators(const char *str1, const char *str2, size_t len) {
    if (len == 0) return 0;

    int32_t c1 = -1, c2 = -1;
    ssize_t len1, len2;

    uint8_t *ptr1 = (uint8_t *)str1;
    uint8_t *ptr2 = (uint8_t *)str2;

    size_t remaining = len;

    size_t match_len = 0;

    bool one_char_match = false;

    while (1) {
        len1 = utf8proc_iterate(ptr1, -1, &c1);
        len2 = utf8proc_iterate(ptr2, -1, &c2);

        if (len1 < 0 && len2 < 0 && *ptr1 == *ptr2) {
            ptr1++;
            ptr2++;
            remaining--;
            match_len++;
            one_char_match = true;
            if (remaining == 0) break;
            continue;
        }

        if (c1 == 0 || c2 == 0) break;

        if (c1 == c2) {
            ptr1 += len1;
            ptr2 += len2;
            remaining -= len1;
            match_len += len1;
            one_char_match = true;
        } else if (utf8_is_hyphen(c1) || utf8_is_separator(utf8proc_category(c1))) {
            ptr1 += len1;
            match_len += len1;
            if (utf8_is_hyphen(c2) || utf8_is_separator(utf8proc_category(c2))) {
                ptr2 += len2;
                remaining -= len2;
            }
        } else if (utf8_is_hyphen(c2) || utf8_is_separator(utf8proc_category(c2))) {
            ptr2 += len2;
            remaining -= len2;
        } else {
            break;
        }

        if (remaining == 0) break;

    }

    return one_char_match ? match_len : 0;

}

inline size_t utf8_common_prefix_ignore_separators(const char *str1, const char *str2) {
    return utf8_common_prefix_len_ignore_separators(str1, str2, strlen(str2));
}

bool utf8_equal_ignore_separators_len(const char *str1, const char *str2, size_t len) {
    if (len == 0) return false;

    int32_t c1 = -1, c2 = -1;
    ssize_t len1, len2;

    uint8_t *ptr1 = (uint8_t *)str1;
    uint8_t *ptr2 = (uint8_t *)str2;

    size_t remaining = len;

    while (1) {
        len1 = utf8proc_iterate(ptr1, -1, &c1);
        len2 = utf8proc_iterate(ptr2, -1, &c2);

        if (len1 < 0 && len2 < 0 && *ptr1 == *ptr2) {
            ptr1++;
            ptr2++;
            remaining--;
            if (remaining == 0) return true;
            continue;
        }

        if (c1 != 0 && c2 != 0 && c1 == c2) {
            ptr1 += len1;
            ptr2 += len2;
            remaining -= len1;
        } else if (utf8_is_hyphen(c1) || utf8_is_separator(utf8proc_category(c1))) {
            ptr1 += len1;
            if (utf8_is_hyphen(c2) || utf8_is_separator(utf8proc_category(c2))) {
                ptr2 += len2;
            }
            remaining -= len1;
        } else if (utf8_is_hyphen(c2) || utf8_is_separator(utf8proc_category(c2))) {
            ptr2 += len2;
            remaining -= len2;
        } else {
            break;
        }

        if (remaining == 0) return true;

    }

    return false;
}

inline bool utf8_equal_ignore_separators(const char *str1, const char *str2) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t len = len1 > len2 ? len1 : len2;

    return utf8_equal_ignore_separators_len(str1, str2, len);
}

bool string_is_digit(char *str, size_t len) {
    uint8_t *ptr = (uint8_t *)str;
    size_t idx = 0;

    bool ignorable = true;

    while (idx < len) {
        int32_t ch;
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (char_len <= 0) break;
        if (ch == 0) break;
        if (!(utf8proc_codepoint_valid(ch))) return false;

        int cat = utf8proc_category(ch);
        if (cat != UTF8PROC_CATEGORY_ND) {
            return false;
        }

        ptr += char_len;
        idx += char_len;
    }

    return true;
}

bool string_is_ignorable(char *str, size_t len) {
    uint8_t *ptr = (uint8_t *)str;
    size_t idx = 0;

    bool ignorable = true;

    while (idx < len) {
        int32_t ch;
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (char_len <= 0) break;
        if (ch == 0) break;
        if (!(utf8proc_codepoint_valid(ch))) return false;

        int cat = utf8proc_category(ch);
        if (!utf8_is_separator(cat) && !utf8_is_hyphen(ch)) {
            return false;
        }

        ptr += char_len;
        idx += char_len;
    }

    return true;
}

ssize_t string_next_hyphen_index(char *str, size_t len) {
    uint8_t *ptr = (uint8_t *)str;
    int32_t codepoint;
    ssize_t idx = 0;

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &codepoint);
        
        if (char_len <= 0 || codepoint == 0) break;

        if (utf8_is_hyphen(codepoint)) return idx;
        ptr += char_len;
        idx += char_len;
    }
    return -1;
}

inline bool string_contains(char *str, char *sub) {
    return str != NULL && sub != NULL && strstr(str, sub) != NULL;
}

inline bool string_contains_hyphen_len(char *str, size_t len) {
    return string_next_hyphen_index(str, len) >= 0;
}

inline bool string_contains_hyphen(char *str) {
    return string_next_hyphen_index(str, strlen(str)) >= 0;
}

ssize_t string_next_codepoint_len(char *str, uint32_t codepoint, size_t len) {
    uint8_t *ptr = (uint8_t *)str;
    int32_t ch;
    ssize_t idx = 0;

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (char_len <= 0 || ch == 0) break;

        if ((uint32_t)ch == codepoint) return idx;
        ptr += char_len;
        idx += char_len;
    }
    return -1;
}

ssize_t string_next_codepoint(char *str, uint32_t codepoint) {
    return string_next_codepoint_len(str, codepoint, strlen(str));
}

ssize_t string_next_period_len(char *str, size_t len) {
    return string_next_codepoint_len(str, PERIOD_CODEPOINT, len);
}

ssize_t string_next_period(char *str) {
    return string_next_codepoint(str, PERIOD_CODEPOINT);
}

inline bool string_contains_period_len(char *str, size_t len) {
    return string_next_codepoint_len(str, PERIOD_CODEPOINT, len) >= 0;
}

inline bool string_contains_period(char *str) {
    return string_next_codepoint(str, string_next_codepoint(str, PERIOD_CODEPOINT)) >= 0;
}

ssize_t string_next_whitespace_len(char *str, size_t len) {
    uint8_t *ptr = (uint8_t *)str;
    int32_t ch;
    ssize_t idx = 0;

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (char_len <= 0 || ch == 0) break;

        if (utf8_is_whitespace(ch)) return idx;
        ptr += char_len;
        idx += char_len;
    }
    return -1;
}

ssize_t string_next_whitespace(char *str) {
    return string_next_whitespace_len(str, strlen(str));
}


size_t string_right_spaces_len(char *str, size_t len) {
    size_t spaces = 0;

    uint8_t *ptr = (uint8_t *)str;
    int32_t ch = 0;
    ssize_t index = len;

    while (1) {
        ssize_t char_len = utf8proc_iterate_reversed(ptr, index, &ch);

        if (ch <= 0) break;

        if (!utf8_is_whitespace(ch)) {
            break;
        }

        index -= char_len;
        spaces += char_len;
    }

    return spaces;

}

inline size_t string_hyphen_prefix_len(char *str, size_t len) {
    // Strip beginning hyphens
    int32_t unichr;
    uint8_t *ptr = (uint8_t *)str;
    ssize_t char_len = utf8proc_iterate(ptr, len, &unichr);
    if (utf8_is_hyphen(unichr)) {
        return (size_t)char_len;
    }
    return 0;
}

inline size_t string_hyphen_suffix_len(char *str, size_t len) {
    // Strip ending hyphens
    int32_t unichr;
    uint8_t *ptr = (uint8_t *)str;
    ssize_t char_len = utf8proc_iterate_reversed(ptr, len, &unichr);
    if (utf8_is_hyphen(unichr)) {
        return (size_t)char_len;
    }
    return 0;
}

size_t string_left_spaces_len(char *str, size_t len) {
    size_t spaces = 0;

    uint8_t *ptr = (uint8_t *)str;
    int32_t ch = 0;
    ssize_t index = 0;

    while (1) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (ch <= 0) break;

        if (!utf8_is_whitespace(ch)) {
            break;
        }
        index += char_len;
        ptr += char_len;
        spaces += char_len;
    }

    return spaces;
}

char *string_trim(char *str) {
    size_t len = strlen(str);
    size_t left_spaces = string_left_spaces_len(str, len);
    size_t right_spaces = string_right_spaces_len(str, len);
    char *ret = strndup(str + left_spaces, len - left_spaces - right_spaces);
    return ret;
}

char_array *char_array_from_string(char *str) {
    size_t len = strlen(str);
    char_array *array = char_array_new_size(len+1);
    strcpy(array->a, str);
    array->n = len;
    return array;
}

char_array *char_array_from_string_no_copy(char *str, size_t n) {
    char_array *array = malloc(sizeof(char_array));
    array->a = str;
    array->m = n;
    array->n = n;
    return array;
}

inline char *char_array_get_string(char_array *array) {
    if (array->n == 0 || array->a[array->n - 1] != '\0') {
        char_array_terminate(array);
    }
    return array->a;
}

inline char *char_array_to_string(char_array *array) {
    if (array->n == 0 || array->a[array->n - 1] != '\0') {
        char_array_terminate(array);
    }
    char *a = array->a;
    free(array);
    return a;
}


inline void char_array_strip_nul_byte(char_array *array) {
    if (array->n > 0 && array->a[array->n - 1] == '\0') {
        array->a[array->n - 1] = '\0';
        array->n--;
    }
}

inline size_t char_array_len(char_array *array) {
    if (array->n > 0 && array->a[array->n - 1] == '\0') {
        return array->n - 1;
    } else {
        return array->n;
    }
}

inline void char_array_append(char_array *array, char *str) {
    while(*str) {
        char_array_push(array, *str++);
    }    
}

inline void char_array_append_len(char_array *array, char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char_array_push(array, *str++);
    }
}

inline void char_array_append_reversed_len(char_array *array, char *str, size_t len) {
    int32_t unich;
    ssize_t char_len;

    size_t idx = len;
    uint8_t *ptr = (uint8_t *)str;

    while(idx > 0) {
        char_len = utf8proc_iterate_reversed(ptr, idx, &unich);
        if (char_len <= 0 || unich == 0) break;
        if (!(utf8proc_codepoint_valid(unich))) break;

        idx -= char_len;
        char_array_append_len(array, str + idx, char_len);
    }
}

inline void char_array_append_reversed(char_array *array, char *str) {
    size_t len = strlen(str);
    char_array_append_reversed_len(array, str, len);
}

inline void char_array_terminate(char_array *array) {
    char_array_push(array, '\0');
}

inline void char_array_cat(char_array *array, char *str) {
    char_array_strip_nul_byte(array);
    char_array_append(array, str);
    char_array_terminate(array);
}

inline void char_array_cat_len(char_array *array, char *str, size_t len) {
    char_array_strip_nul_byte(array);
    char_array_append_len(array, str, len);
    char_array_terminate(array);
}


inline void char_array_cat_reversed(char_array *array, char *str) {
    char_array_strip_nul_byte(array);
    char_array_append_reversed(array, str);
    char_array_terminate(array);
}


inline void char_array_cat_reversed_len(char_array *array, char *str, size_t len) {
    char_array_strip_nul_byte(array);
    char_array_append_reversed_len(array, str, len);
    char_array_terminate(array);
}

inline void char_array_add(char_array *array, char *str) {
    char_array_append(array, str);
    char_array_terminate(array);
}

inline void char_array_add_len(char_array *array, char *str, size_t len) {
    char_array_append_len(array, str, len);
    char_array_terminate(array);
}


void char_array_add_vjoined(char_array *array, char *separator, bool strip_separator, int count, va_list args) {
    if (count <= 0) {
        return;        
    }

    size_t separator_len = strlen(separator);

    for (size_t i = 0; i < count - 1; i++) {
        char *arg = va_arg(args, char *);
        size_t len = strlen(arg);

        if (strip_separator && 
            ((separator_len == 1 && arg[len-1] == separator[0]) || 
            (len > separator_len && strncmp(arg + len - separator_len, separator, separator_len) == 0))) {
            len -= separator_len;
        }

        char_array_append_len(array, arg, len);
        char_array_append(array, separator);
    }

    char *arg = va_arg(args, char *);
    char_array_append(array, arg);
    char_array_terminate(array);

}

inline void char_array_add_joined(char_array *array, char *separator, bool strip_separator, int count, ...) {
    va_list args;
    va_start(args, count);
    char_array_add_vjoined(array, separator, strip_separator, count, args);
    va_end(args);
}

inline void char_array_cat_joined(char_array *array, char *separator, bool strip_separator, int count, ...) {
    char_array_strip_nul_byte(array);
    va_list args;
    va_start(args, count);
    char_array_add_vjoined(array, separator, strip_separator, count, args);
    va_end(args);
}

void char_array_cat_vprintf(char_array *array, char *format, va_list args) {
    char_array_strip_nul_byte(array);

    va_list cpy;

    char *buf;
    size_t buflen;

    size_t last_n = array->n;
    size_t size = array->m - array->n <= 2 ? array->m * 2 : array->m;

    while(1) {
        char_array_resize(array, size);
        buf = array->a + last_n;
        buflen = size - last_n;
        if (buf == NULL) return;
        array->a[size - 2] = '\0';
        va_copy(cpy, args);
        vsnprintf(buf, buflen, format, cpy);
        if (array->a[size - 2] != '\0') {
            size *= 2;
            continue;
        } else {
            array->n += strlen(buf);
        }
        break;
    }
}

void char_array_cat_printf(char_array *array, char *format, ...) {
    va_list args;
    va_start(args, format);
    char_array_cat_vprintf(array, format, args);
    va_end(args);
}

cstring_array *cstring_array_new(void) {
    cstring_array *array = malloc(sizeof(cstring_array));
    if (array == NULL) return NULL;

    array->indices = uint32_array_new();
    if (array->indices == NULL) {
        cstring_array_destroy(array);
        return NULL;
    }

    array->str = char_array_new();
    if (array->str == NULL) {
        cstring_array_destroy(array);
        return NULL;
    }

    return array;
}

void cstring_array_destroy(cstring_array *self) {
    if (self == NULL) return;
    if (self->indices) {
        uint32_array_destroy(self->indices);
    }
    if (self->str) {
        char_array_destroy(self->str);
    }
    free(self);
}

cstring_array *cstring_array_new_size(size_t size) {
    cstring_array *array = cstring_array_new();
    char_array_resize(array->str, size);
    return array;
}

cstring_array *cstring_array_from_char_array(char_array *str) {
    if (str == NULL) return NULL;
    if (str->n == 0)
        return cstring_array_new();

    cstring_array *array = malloc(sizeof(cstring_array));
    if (array == NULL) return NULL;

    array->str = str;
    array->indices = uint32_array_new_size(1);

    uint32_array_push(array->indices, 0);
    char *ptr = str->a;
    for (uint32_t i = 0; i < str->n - 1; i++, ptr++) {
        if (*ptr == '\0') {
            uint32_array_push(array->indices, i + 1);
        }
    }
    return array;
}

cstring_array *cstring_array_from_strings(char **strings, size_t n) {
    cstring_array *array = cstring_array_new();
    for (size_t i = 0; i < n; i++) {
        cstring_array_start_token(array);
        cstring_array_add_string(array, strings[i]);
    }
    return array;
}

bool cstring_array_extend(cstring_array *array, cstring_array *other) {
    if (array == NULL || other == NULL) return false;
    size_t n = cstring_array_num_strings(other);

    for (size_t i = 0; i < n; i++) {
        char *s_i = cstring_array_get_string(other, i);
        cstring_array_add_string(array, s_i);
    }
    return true;
}


inline size_t cstring_array_capacity(cstring_array *self) {
    return self->str->m;
}

inline size_t cstring_array_used(cstring_array *self) {
    return self->str->n;
}

inline size_t cstring_array_num_strings(cstring_array *self) {
    if (self == NULL) return 0;
    return self->indices->n;
}

inline void cstring_array_resize(cstring_array *self, size_t size) {
    if (size < cstring_array_capacity(self)) return;
    char_array_resize(self->str, size);
}

void cstring_array_clear(cstring_array *self) {
    if (self == NULL) return;

    if (self->indices != NULL) {
        uint32_array_clear(self->indices);
    }

    if (self->str != NULL) {
        char_array_clear(self->str);
    }
}

inline uint32_t cstring_array_start_token(cstring_array *self) {
    uint32_t index = (uint32_t)self->str->n;
    uint32_array_push(self->indices, index);
    return index;
}

inline void cstring_array_terminate(cstring_array *self) {
    char_array_terminate(self->str);
}

inline uint32_t cstring_array_add_string(cstring_array *self, char *str) {
    uint32_t index = cstring_array_start_token(self);
    char_array_append(self->str, str);
    char_array_terminate(self->str);
    return index;
}

inline uint32_t cstring_array_add_string_len(cstring_array *self, char *str, size_t len) {
    uint32_t index = cstring_array_start_token(self);
    char_array_append_len(self->str, str, len);
    char_array_terminate(self->str);
    return index;
}

inline void cstring_array_append_string(cstring_array *self, char *str) {
    char_array_append(self->str, str);
}

inline void cstring_array_append_string_len(cstring_array *self, char *str, size_t len) {
    char_array_append_len(self->str, str, len);
}

inline void cstring_array_cat_string(cstring_array *self, char *str) {
    char_array_cat(self->str, str);
}

inline void cstring_array_cat_string_len(cstring_array *self, char *str, size_t len) {
    char_array_cat_len(self->str, str, len);
}

inline int32_t cstring_array_get_offset(cstring_array *self, uint32_t i) {
    if (INVALID_INDEX(i, self->indices->n)) {
        return -1;
    }
    return (int32_t)self->indices->a[i];
}

inline char *cstring_array_get_string(cstring_array *self, uint32_t i) {
    int32_t data_index = cstring_array_get_offset(self, i);
    if (data_index < 0) return NULL;
    return self->str->a + data_index;
}

inline int64_t cstring_array_token_length(cstring_array *self, uint32_t i) {
    if (INVALID_INDEX(i, self->indices->n)) {
        return -1;
    }
    if (i < self->indices->n - 1) {
        return self->indices->a[i+1] - self->indices->a[i] - 1;
    } else {        
        return self->str->n - self->indices->a[i] - 1;
    }
}

static cstring_array *cstring_array_split_options(char *str, const char *separator, size_t separator_len, bool ignore_consecutive, size_t *count) {
    *count = 0;
    char_array *array = char_array_new_size(strlen(str));

    bool last_was_separator = false;
    bool first_char = false;

    while (*str) {
        if ((separator_len == 1 && *str == separator[0]) || (memcmp(str, separator, separator_len) == 0)) {
            if (first_char && (!ignore_consecutive || !last_was_separator)) {
                char_array_push(array, '\0');
            }
            str += separator_len;
            last_was_separator = true;
        } else {
            char_array_push(array, *str);
            str++;
            last_was_separator = false;
            first_char = true;
        }
    }
    char_array_push(array, '\0');

    cstring_array *string_array = cstring_array_from_char_array(array);
    *count = cstring_array_num_strings(string_array);

    return string_array;
}


cstring_array *cstring_array_split(char *str, const char *separator, size_t separator_len, size_t *count) {
    return cstring_array_split_options(str, separator, separator_len, false, count);
}


cstring_array *cstring_array_split_ignore_consecutive(char *str, const char *separator, size_t separator_len, size_t *count) {
    return cstring_array_split_options(str, separator, separator_len, true, count);
}


cstring_array *cstring_array_split_no_copy(char *str, char separator, size_t *count) {
    *count = 0;
    char *ptr = str;
    size_t len = strlen(str);

    for (int i = 0; i < len; i++, ptr++) {
        if (*ptr == separator) {
            *ptr = '\0';
        }
    }

    char_array *array = char_array_from_string_no_copy(str, len);
    cstring_array *string_array = cstring_array_from_char_array(array);
    *count = cstring_array_num_strings(string_array);

    return string_array;
}


char **cstring_array_to_strings(cstring_array *self) {
    char **strings = malloc(self->indices->n * sizeof(char *));

    for (int i = 0; i < cstring_array_num_strings(self); i++) {
        char *str = cstring_array_get_string(self, i);
        strings[i] = strdup(str);
    }

    cstring_array_destroy(self);
    return strings;
}


string_tree_t *string_tree_new_size(size_t size) {
    string_tree_t *self = malloc(sizeof(string_tree_t));
    if (self == NULL) {
        return NULL;
    }

    self->token_indices = uint32_array_new_size(size);
    if (self->token_indices == NULL) {
        free(self);
        return NULL;
    }

    uint32_array_push(self->token_indices, 0);

    self->strings = cstring_array_new();
    if (self->strings == NULL) {
        uint32_array_destroy(self->token_indices);
        free(self);
        return NULL;
    }

    return self;
}

#define DEFAULT_STRING_TREE_SIZE 8

string_tree_t *string_tree_new(void) {
    return string_tree_new_size((size_t)DEFAULT_STRING_TREE_SIZE);
}

inline char *string_tree_get_alternative(string_tree_t *self, size_t token_index, uint32_t alternative) {
    if (token_index >= self->token_indices->n) return NULL;

    uint32_t token_start = self->token_indices->a[token_index];

    return cstring_array_get_string(self->strings, token_start + alternative);
}

inline void string_tree_finalize_token(string_tree_t *self) {
    uint32_array_push(self->token_indices, (uint32_t)cstring_array_num_strings(self->strings));
}

void string_tree_clear(string_tree_t *self) {
    uint32_array_clear(self->token_indices);
    uint32_array_push(self->token_indices, 0);
    cstring_array_clear(self->strings);
}

// terminated
inline void string_tree_add_string(string_tree_t *self, char *str) {
    cstring_array_add_string(self->strings, str);
}

inline void string_tree_add_string_len(string_tree_t *self, char *str, size_t len) {
    cstring_array_add_string_len(self->strings, str, len);
}

// unterminated
inline void string_tree_append_string(string_tree_t *self, char *str) {
    cstring_array_append_string(self->strings, str);
}

inline void string_tree_append_string_len(string_tree_t *self, char *str, size_t len) {
    cstring_array_append_string_len(self->strings, str, len);
}

inline uint32_t string_tree_num_tokens(string_tree_t *self) {
    return (uint32_t)self->token_indices->n - 1;
}

inline uint32_t string_tree_num_strings(string_tree_t *self) {
    return (uint32_t)cstring_array_num_strings(self->strings);
}

inline uint32_t string_tree_num_alternatives(string_tree_t *self, uint32_t i) {
    if (i >= self->token_indices->n) return 0;
    uint32_t n = self->token_indices->a[i + 1] - self->token_indices->a[i];
    return n > 0 ? n : 1;
}

void string_tree_destroy(string_tree_t *self) {
    if (self == NULL) return;

    if (self->token_indices != NULL) {
        uint32_array_destroy(self->token_indices);
    }

    if (self->strings != NULL) {
        cstring_array_destroy(self->strings);
    }

    free(self);
}

string_tree_iterator_t *string_tree_iterator_new(string_tree_t *tree) {
    string_tree_iterator_t *self = malloc(sizeof(string_tree_iterator_t));
    self->tree = tree;

    uint32_t num_tokens = string_tree_num_tokens(tree);
    self->num_tokens = num_tokens;

    // calloc since the first path through the tree is all zeros
    self->path = calloc(num_tokens, sizeof(uint32_t));

    uint32_t permutations = 1;
    uint32_t num_strings;

    for (int i = 0; i < num_tokens; i++) {
        // N + 1 indices stored in token_indices, so this is always valid
        num_strings = string_tree_num_alternatives(tree, i);
        if (num_strings > 0) {
            // 1 or more strings in the string_tree means use those instead of the actual token
            permutations *= num_strings;
        }
    }

    if (permutations > 1) {
        self->remaining = (uint32_t)permutations;
    } else{
        self->remaining = 1;
    }

    return self;
}

void string_tree_iterator_next(string_tree_iterator_t *self) {
    if (self->remaining > 0) {
        int i;
        for (i = self->num_tokens - 1; i >= 0; i--) {
            self->path[i]++;
            if (self->path[i] == string_tree_num_alternatives(self->tree, i)) {
                self->path[i] = 0;
            } else {
                self->remaining--;
                break;
            }
        }

        if (i < 0) {
            self->remaining = 0;
        }
    }
}

char *string_tree_iterator_get_string(string_tree_iterator_t *self, uint32_t i) {
    if (i >= self->num_tokens) {
        return NULL;
    }
    uint32_t base_index = self->tree->token_indices->a[i];
    uint32_t offset = self->path[i];

    return cstring_array_get_string(self->tree->strings, base_index + offset);
}

bool string_tree_iterator_done(string_tree_iterator_t *self) {
    return self->remaining == 0;
}

void string_tree_iterator_destroy(string_tree_iterator_t *self) {
    if (self == NULL) return;

    if (self->path) {
        free(self->path);
    }

    free(self);
}
