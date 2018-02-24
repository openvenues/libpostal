/*
string_utils.h
--------------

Utilities for manipulating strings in C.
*/
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include "collections.h"
#include "utf8proc/utf8proc.h"
#include "vector.h"

#define MAX_UTF8_CHAR_SIZE 4

#define INT8_MAX_STRING_LEN 5
#define INT8_MAX_STRING_SIZE INT8_MAX_STRING_LEN + 1
#define INT16_MAX_STRING_LEN 7
#define INT16_MAX_STRING_SIZE INT16_MAX_STRING_LEN + 1
#define INT32_MAX_STRING_LEN 11
#define INT32_MAX_STRING_SIZE INT32_MAX_STRING_LEN + 1
#define INT64_MAX_STRING_LEN 21
#define INT64_MAX_STRING_SIZE INT64_MAX_STRING_LEN + 1


#define UTF8PROC_OPTIONS_BASE UTF8PROC_NULLTERM | UTF8PROC_STABLE

// Unicode normalization forms
#define UTF8PROC_OPTIONS_NFD UTF8PROC_OPTIONS_BASE | UTF8PROC_DECOMPOSE
#define UTF8PROC_OPTIONS_NFC UTF8PROC_OPTIONS_BASE | UTF8PROC_COMPOSE
#define UTF8PROC_OPTIONS_NFKD UTF8PROC_OPTIONS_NFD | UTF8PROC_COMPAT
#define UTF8PROC_OPTIONS_NFKC UTF8PROC_OPTIONS_NFC | UTF8PROC_COMPAT

// Strip accents
#define UTF8PROC_OPTIONS_STRIP_ACCENTS UTF8PROC_OPTIONS_BASE | UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK

// Lowercase
#define UTF8PROC_OPTIONS_CASE_FOLD UTF8PROC_OPTIONS_BASE | UTF8PROC_CASEFOLD


// ASCII string methods
int string_compare_case_insensitive(const char *str1, const char *str2);
int string_compare_len_case_insensitive(const char *str1, const char *str2, size_t len);
size_t string_common_prefix(const char *str1, const char *str2);
size_t string_common_suffix(const char *str1, const char *str2);

bool string_is_lower(char *s);
void string_lower(char *s);
bool string_is_upper(char *s);
void string_upper(char *s);

char *string_replace_char(char *str, char c1, char c2);
bool string_replace_with_array(char *str, char *replace, char *with, char_array *result);
char *string_replace(char *str, char *replace, char *with);

bool string_starts_with(const char *str, const char *start);
bool string_ends_with(const char *str, const char *ending);

bool string_equals(const char *s1, const char *s2);

uint32_t string_translate(char *str, size_t len, char *word_chars, char *word_repls, size_t trans_len);

// UTF-8 string methods
char *utf8_reversed_string(const char *s); // returns a copy, caller frees
ssize_t utf8proc_iterate_reversed(const uint8_t *str, ssize_t start, int32_t *dst);

// Casing functions return a copy, caller frees
char *utf8_lower_options(const char *s, utf8proc_option_t options);
char *utf8_lower(const char *s);
char *utf8_upper_options(const char *s, utf8proc_option_t options);
char *utf8_upper(const char *s);

int utf8_compare(const char *str1, const char *str2);
int utf8_compare_len(const char *str1, const char *str2, size_t len);
int utf8_compare_case_insensitive(const char *str1, const char *str2, size_t len);
int utf8_compare_len_case_insensitive(const char *str1, const char *str2, size_t len);
size_t utf8_common_prefix(const char *str1, const char *str2);
size_t utf8_common_prefix_len(const char *str1, const char *str2, size_t len);
size_t utf8_common_prefix_ignore_separators(const char *str1, const char *str2);
size_t utf8_common_prefix_len_ignore_separators(const char *str1, const char *str2, size_t len);

bool utf8_equal_ignore_separators(const char *str1, const char *str2);

ssize_t utf8_len(const char *str, size_t len);

uint32_array *unicode_codepoints(const char *str);
bool unicode_equals(uint32_array *u1_array, uint32_array *u2_array);
size_t unicode_common_prefix(uint32_array *u1_array, uint32_array *u2_array);
size_t unicode_common_suffix(uint32_array *u1_array, uint32_array *u2_array);

bool utf8_is_hyphen(int32_t ch);
bool utf8_is_period(int32_t ch);
bool utf8_is_letter(int cat);
bool utf8_is_number(int cat);
bool utf8_is_digit(int cat);
bool utf8_is_letter_or_number(int cat);
bool utf8_is_punctuation(int cat);
bool utf8_is_symbol(int cat);
bool utf8_is_separator(int cat);
bool utf8_is_whitespace(int32_t ch);

bool string_is_digit(char *str, size_t len);
bool string_is_ignorable(char *str, size_t len);

ssize_t string_next_hyphen_index(char *str, size_t len);
bool string_contains(char *str, char *sub);
bool string_contains_hyphen(char *str);
bool string_contains_hyphen_len(char *str, size_t len);

ssize_t string_next_codepoint_len(char *str, uint32_t codepoint, size_t len);
ssize_t string_next_codepoint(char *str, uint32_t codepoint);

ssize_t string_next_period_len(char *str, size_t len);
ssize_t string_next_period(char *str);

bool string_contains_period_len(char *str, size_t len);
bool string_contains_period(char *str);

ssize_t string_next_whitespace_len(char *str, size_t len);
ssize_t string_next_whitespace(char *str);

size_t string_left_spaces_len(char *str, size_t len);
size_t string_right_spaces_len(char *str, size_t len);
char *string_trim(char *str);

size_t string_hyphen_prefix_len(char *str, size_t len);
size_t string_hyphen_suffix_len(char *str, size_t len);

/* char_array is a dynamic character array defined in collections.h
but has a few additional methods related to string manipulation.

The array pointer can be treated as a plain old C string for methods
expecting NUL-terminated char pointers, but operations like 
concatenation are cheap and safe.
*/
char_array *char_array_from_string(char *str);
char_array *char_array_from_string_no_copy(char *str, size_t n);

// Gets the underlying C string for a char_array
char *char_array_get_string(char_array *array);

// Frees the char_array and returns a standard NUL-terminated string
char *char_array_to_string(char_array *array);

// Can use strlen(array->a) but this is faster
size_t char_array_len(char_array *array);

// append_* methods do not NUL-terminate
void char_array_append(char_array *array, char *str);
void char_array_append_len(char_array *array, char *str, size_t len);
void char_array_append_reversed(char_array *array, char *str);
void char_array_append_reversed_len(char_array *array, char *str, size_t len);
// add NUL terminator to a char_array
void char_array_strip_nul_byte(char_array *array);
void char_array_terminate(char_array *array);

// add_* methods NUL-terminate without stripping NUL-byte
void char_array_add(char_array *array, char *str);
void char_array_add_len(char_array *array, char *str, size_t len);

// Similar to strcat but with dynamic resizing, guaranteed NUL-terminated
void char_array_cat(char_array *array, char *str);
void char_array_cat_len(char_array *array, char *str, size_t len);
void char_array_cat_reversed(char_array *array, char *str);
void char_array_cat_reversed_len(char_array *array, char *str, size_t len);

// Similar to cat methods but with printf args
void char_array_cat_vprintf(char_array *array, char *format, va_list args);
void char_array_cat_printf(char_array *array, char *format, ...);

// Mainly for paths or delimited strings
void char_array_add_vjoined(char_array *array, char *separator, bool strip_separator, int count, va_list args);
void char_array_add_joined(char_array *array, char *separator, bool strip_separator, int count, ...);
void char_array_cat_joined(char_array *array, char *separator, bool strip_separator, int count, ...);


/*
cstring_arrays represent n strings stored contiguously, delimited by the NUL byte.

Instead of storing an array of char pointers (char **), cstring_arrays use this format:

array->indices = {0, 4, 9};
array->str = {'f', 'o', 'o', '\0', 'b', 'a', 'r', '\0', 'b', 'a', 'z', '\0'};

Each value in array->indices is the start position of a token in array->str. Each string
is NUL-terminated, so array->str->a + 4 is "bar", a valid NUL-terminated C string

array->str is a char_array, so all of the powerful methods like char_array_cat_printf above
can be used when building the contiguous string arrays as well.

*/

typedef struct {
    uint32_array *indices;
    char_array *str;
} cstring_array;

cstring_array *cstring_array_new(void);

cstring_array *cstring_array_new_size(size_t size);

size_t cstring_array_capacity(cstring_array *self);
size_t cstring_array_used(cstring_array *self);
size_t cstring_array_num_strings(cstring_array *self);
void cstring_array_resize(cstring_array *self, size_t size);
void cstring_array_clear(cstring_array *self);

cstring_array *cstring_array_from_char_array(char_array *str);
cstring_array *cstring_array_from_strings(char **strings, size_t n);

bool cstring_array_extend(cstring_array *array, cstring_array *other);

// Convert cstring_array to an array of n C strings and destroy the cstring_array
char **cstring_array_to_strings(cstring_array *self);

// Split on delimiter
cstring_array *cstring_array_split(char *str, const char *separator, size_t separator_len, size_t *count);
// Split on delimiter, ignore multiple consecutive delimiters
cstring_array *cstring_array_split_ignore_consecutive(char *str, const char *separator, size_t separator_len, size_t *count);

// Split on delimiter by replacing (single character) separator with the NUL byte in the original string
cstring_array *cstring_array_split_no_copy(char *str, char separator, size_t *count);

uint32_t cstring_array_start_token(cstring_array *self);
uint32_t cstring_array_add_string(cstring_array *self, char *str);
uint32_t cstring_array_add_string_len(cstring_array *self, char *str, size_t len);

void cstring_array_append_string(cstring_array *self, char *str);
void cstring_array_append_string_len(cstring_array *self, char *str, size_t len);

void cstring_array_cat_string(cstring_array *self, char *str);
void cstring_array_cat_string_len(cstring_array *self, char *str, size_t len);

void cstring_array_terminate(cstring_array *self);
int32_t cstring_array_get_offset(cstring_array *self, uint32_t i);
char *cstring_array_get_string(cstring_array *self, uint32_t i);
int64_t cstring_array_token_length(cstring_array *self, uint32_t i); 

void cstring_array_destroy(cstring_array *self);

#define cstring_array_foreach(array, i, s, code) {                                      \
    for (int __si = 0; __si < array->indices->n; __si++) {                              \
        (i) = __si;                                                                     \
        (s) = array->str->a + array->indices->a[__si];                                  \
        code;                                                                           \
    }                                                                                   \
}

/*
String trees are a way of storing alternative representations of a tokenized string concisely

Particularly with hyphens, we may want the string "twenty-five" to normalize to both:

twenty five
twentyfive

so when we encounter "twenty-five", we'd propose both alternative representations as possible
normalizations of the token.

string_tree is similar to a CSR (compressed sparse row) sparse matrix.

@tokens - for token i, tree->tokens[i] is the index in strings->indices where token i's alternatives begin
@strings - a contiguous string array which only contains as many tokens as there are alternatives

Since we typically only normalize on mid-word hyphens, periods and non-ASCII characters, a string_tree
might not need to store anything at all in many languages.

*/

typedef struct string_tree {
    uint32_array *token_indices;
    cstring_array *strings;
} string_tree_t;

string_tree_t *string_tree_new(void);
string_tree_t *string_tree_new_size(size_t size);

// get
char *string_tree_get_alternative(string_tree_t *self, size_t token_index, uint32_t alternative);

// finalize
void string_tree_finalize_token(string_tree_t *self);
// terminated
void string_tree_add_string(string_tree_t *self, char *str);
void string_tree_add_string_len(string_tree_t *self, char *str, size_t len);
// unterminated
void string_tree_append_string(string_tree_t *self, char *str);
void string_tree_append_string_len(string_tree_t *self, char *str, size_t len);

void string_tree_clear(string_tree_t *self);

uint32_t string_tree_num_tokens(string_tree_t *self);
uint32_t string_tree_num_strings(string_tree_t *self);

uint32_t string_tree_num_alternatives(string_tree_t *self, uint32_t i);

void string_tree_destroy(string_tree_t *self);

typedef struct string_tree_iterator {
    string_tree_t *tree;
    uint32_t *path;
    uint32_t num_tokens;
    uint32_t remaining;
} string_tree_iterator_t;

string_tree_iterator_t *string_tree_iterator_new(string_tree_t *tree);
void string_tree_iterator_next(string_tree_iterator_t *self);
char *string_tree_iterator_get_string(string_tree_iterator_t *self, uint32_t i);
bool string_tree_iterator_done(string_tree_iterator_t *self);
void string_tree_iterator_destroy(string_tree_iterator_t *self);


#define string_tree_iterator_foreach_token(iter, s, code) {                             \
    string_tree_t *tree = iter->tree;                                                   \
    for (int __pi = 0; __pi < iter->num_tokens; __pi++) {                               \
        (s) = string_tree_get_alternative(tree, __pi, iter->path[__pi]);                \
        code;                                                                           \
    }                                                                                   \
}


 

#endif
