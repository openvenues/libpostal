#include "string_utils.h"
#include <stdio.h>


#define INVALID_INDEX(i, n) ((i) < 0 || (i) >= (n) - 1)

int string_compare_case_insensitive(const char *str1, const char *str2) {
    int c1, c2;

    do {
        c1 =  tolower(*str1++);
        c2 = tolower(*str2++);
    } while (c1 == c2 && c1 != 0);

    return c1 - c2;
}

int string_compare_n_case_insensitive(const char *str1, const char *str2, size_t len) {
    register unsigned char *s1 = (unsigned char *) str1;
    register unsigned char *s2 = (unsigned char *) str2;

    unsigned char c1, c2;

    if (!len)
        return 0;

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

int string_common_prefix(const char *str1, const char *str2) {
    int common_prefix;
    for (common_prefix = 0; *str1 && *str2 && *str1 == *str2; str1++, str2++)
        common_prefix++;
    return common_prefix;
}

int string_common_suffix(const char *str1, const char *str2) {
    int common_suffix = 0;
    int str1_len = strlen(str1);
    int str2_len = strlen(str2);
    int min_len = (str1_len < str2_len) ? str1_len : str2_len;
    for (int i=1; i <= min_len && str1[str1_len-i] == str2[str2_len-i]; i++)
        common_suffix++;
    return common_suffix;
}

bool string_starts_with(const char *str, const char *start) {
    for (; *start; str++, start++)
        if (*str != *start)
            return false;
    return true;
}

bool string_ends_with(const char *str, const char *ending) {
    int end_len = strlen(ending);
    int str_len = strlen(str);

    return str_len < end_len ? false : !strcmp(str + str_len - end_len, ending);
}

void string_upper(char *s) {
    for (; *s; ++s) *s = toupper(*s);
}

void string_lower(char *s) {
    for (; *s; ++s) *s = tolower(*s);
}

uint string_translate(sds str, char *word_chars, char *word_repls, size_t trans_len) {
    uint num_replacements = 0;
    size_t len = sdslen(str);
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

bool utf8_is_letter(int32_t ch) {
    const utf8proc_property_t *props = utf8proc_get_property(ch);
    utf8proc_propval_t cat = props->category;
    return cat == UTF8PROC_CATEGORY_LL || cat == UTF8PROC_CATEGORY_LU        \
            || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LO || \
            cat == UTF8PROC_CATEGORY_LM;

}

char *string_strip_whitespace(char *str) {
    char *end;

    while (isspace(*str)) str++;

    if (*str == '\0')
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;

    *(end+1) = '\0';

    return str;
}

void contiguous_string_array_add_string_unterminated(char_array *array, char *str) {
    while (*str) {
        char_array_push(array, *str++);
    }    
}

void contiguous_string_array_add_string(char_array *array, char *str) {
    contiguous_string_array_add_string_unterminated(array, str);
    char_array_push(array, '\0');
}

void contiguous_string_array_add_string_unterminated_len(char_array *array, char *str, size_t len) {
    for (int i = 0; i < len; i++) {
        char_array_push(array, *str++);
    }
}

void contiguous_string_array_add_string_len(char_array *array, char *str, size_t len) {
    contiguous_string_array_add_string_unterminated_len(array, str, len);
    char_array_push(array, '\0');
}

// Designed for using the char_array and uchar_array to store lots of short strings
int contiguous_string_array_next_index(char_array *string_array, int i, size_t n) {
    if (INVALID_INDEX(i, string_array->n)) {
        return -1;
    }

    int len = 0;
    char *array = string_array->a + i;


    while (*array && i + len <= n - 1) {
        array++; 
        len++;
    }

    if (len < n - 1) {
        return len + 1;
    }

    return -1;

}
