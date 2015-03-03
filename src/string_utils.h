#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "collections.h"
#include "sds/sds.h"
#include "utf8proc/utf8proc.h"
#include "vector.h"

VECTOR_INIT_FREE_DATA(string_array, sds, sdsfree)


// NOTE: this particular implementation works only for ASCII strings
int string_compare_case_insensitive(const char *str1, const char *str2);
int string_compare_n_case_insensitive(const char *str1, const char *str2, size_t len);
int string_common_prefix(const char *str1, const char *str2);

void string_lower(char *str);
void string_upper(char *str);

bool string_starts_with(const char *str, const char *start);
bool string_ends_with(const char *str, const char *ending);

uint string_translate(sds str, char *word_chars, char *word_repls, size_t trans_len);

char *utf8_reversed_string(const char *s); // returns a copy, caller frees
bool utf8_is_letter(int32_t ch);

char *string_strip_whitespace(char *str);

void contiguous_string_array_add_string_unterminated(char_array *array, char *str);
void contiguous_string_array_add_string(char_array *array, char *str);
void contiguous_string_array_add_string_unterminated_len(char_array *array, char *str, size_t len);
void contiguous_string_array_add_string_len(char_array *array, char *str, size_t len);

int contiguous_string_array_next_index(char_array *string_array, int i, size_t n);


#ifdef __cplusplus
}
#endif

#endif