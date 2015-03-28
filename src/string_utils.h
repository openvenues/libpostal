#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include "collections.h"
#include "sds/sds.h"
#include "utf8proc/utf8proc.h"
#include "vector.h"

// NOTE: this particular implementation works only for ASCII strings
int string_compare_case_insensitive(const char *str1, const char *str2);
int string_compare_n_case_insensitive(const char *str1, const char *str2, size_t len);
int string_common_prefix(const char *str1, const char *str2);

void string_lower(char *str);
void string_upper(char *str);

bool string_starts_with(const char *str, const char *start);
bool string_ends_with(const char *str, const char *ending);

uint string_translate(char *str, size_t len, char *word_chars, char *word_repls, size_t trans_len);

char *utf8_reversed_string(const char *s); // returns a copy, caller frees
bool utf8_is_letter(int32_t ch);

char *string_strip_whitespace(char *str);

/* Caller has to free the original string,
   also keep in mind that after operating on a char array,
   the pointer to the original string may get realloc'd and change
   so need to set the char pointer to array.a when done.
   Consider a macro which does this consistently
*/
char_array *char_array_from_string(char *str);
char *char_array_to_string(char_array *array, bool free_array);

void char_array_append(char_array *array, char *str);
void char_array_append_len(char_array *array, char *str, size_t len);
void char_array_terminate(char_array *array);

// Similar to strcat, strips NUL-byte and guarantees 0-terminated
void char_array_cat(char_array *array, char *str);
void char_array_cat_len(char_array *array, char *str, size_t len);

// Strips NUL-byte but does not NUL-terminate
void char_array_cat_unterminated(char_array *array, char *str);
void char_array_cat_unterminated_len(char_array *array, char *str, size_t len);

// Cat with printf args
void char_array_cat_printf(char_array *array, char *format, ...);

void char_array_add_joined(char_array *array, char *separator, int count, ...);
void char_array_cat_joined(char_array *array, char *separator, int count, ...);


/*
cstring_arrays represent n strings stored contiguously, delimited by NUL-byte.

Instead of storing an array of char pointers (char **), cstring_arrays use this format:

array->indices = {0, 4, 9};
array->str = {'f', 'o', 'o', '\0', 'b', 'a', 'r', '\0', 'b', 'a', 'z', '\0'};

*/

typedef struct {
    uint32_array *indices;
    char_array *str;
} cstring_array;

cstring_array *cstring_array_new(void);

cstring_array *cstring_array_new_size(size_t size);

cstring_array *cstring_array_from_char_array(char_array *str);

cstring_array *cstring_array_split(char *str, const char *separator, size_t separator_len, int *count);

void cstring_array_join_strings(cstring_array *self, char *separator, int count, ...);
void cstring_array_start_token(cstring_array *self);
void cstring_array_add_string(cstring_array *self, char *s);
void cstring_array_add_string_len(cstring_array *self, char *s, size_t len);
int32_t cstring_array_get_offset(cstring_array *self, uint32_t i);
char *cstring_array_get_token(cstring_array *self, uint32_t i);

void cstring_array_destroy(cstring_array *self);

#ifdef __cplusplus
}
#endif

#endif
