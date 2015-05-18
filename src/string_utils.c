#include <stdio.h>

#include "string_utils.h"

#define INVALID_INDEX(i, n) ((i) < 0 || (i) >= (n))

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
    size_t str1_len = strlen(str1);
    size_t str2_len = strlen(str2);
    size_t min_len = (str1_len < str2_len) ? str1_len : str2_len;
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

uint string_translate(char *str, size_t len, char *word_chars, char *word_repls, size_t trans_len) {
    uint num_replacements = 0;
    
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

ssize_t utf8proc_iterate_reversed(const uint8_t *str, const uint8_t *start, int32_t *dst) {
    ssize_t len = 0;

    const uint8_t *ptr = str;

    *dst = -1;

    do {
        if (ptr <= start) return 0;
        ptr--; len++;
    } while ((*ptr & 0xC0) == 0x80);

    return utf8proc_iterate(ptr, len, dst);
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

size_t string_rtrim(char *str) {
    size_t spaces = 0;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        spaces++;
        end--;
    }

    *(end+1) = '\0';

    return spaces;
}

size_t string_ltrim(char *str) {
    char *end;

    size_t spaces = 0;

    size_t len = strlen(str) - 1;
    char *ptr = str;

    while (isspace(*ptr++)) {
        spaces++;
    }

    if (spaces > 0) {
        memmove(str, str + spaces, len + 1 - spaces);
    }

    return spaces;
}

size_t string_trim(char *str) {
    size_t spaces = string_ltrim(str);
    spaces += string_rtrim(str);
    return spaces;
}

char_array *char_array_from_string(char *str) {
    size_t len = strlen(str);
    char_array *array = char_array_new_size(len+1);
    strcpy(array->a, str);
    array->n = strlen(str);
    return array;
}

inline char *char_array_get_string(char_array *array) {
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


static inline void char_array_strip_nul_byte(char_array *array) {
    if (array->n > 0 && array->a[array->n - 1] == '\0') {
        array->a[array->n - 1] = '\0';
        array->n--;
    }
}

size_t char_array_len(char_array *array) {
    if (array->n > 0 && array->a[array->n - 1] == '\0') {
        return array->n - 1;
    } else {
        return array->n;
    }
}

void char_array_append(char_array *array, char *str) {
    while(*str) {
        char_array_push(array, *str++);
    }    
}

void char_array_append_len(char_array *array, char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char_array_push(array, *str++);
    }
}

void char_array_terminate(char_array *array) {
    char_array_push(array, '\0');
}

char_array *char_array_copy(char_array *array) {
    char_array *copy = char_array_new_size(array->m);
    memcpy(copy->a, array->a, array->n);
    copy->n = array->n;
    return copy;
}

void char_array_cat(char_array *array, char *str) {
    char_array_strip_nul_byte(array);
    char_array_append(array, str);
    char_array_terminate(array);
}

void char_array_cat_len(char_array *array, char *str, size_t len) {
    char_array_strip_nul_byte(array);
    char_array_append_len(array, str, len);
    char_array_terminate(array);
}

void char_array_add(char_array *array, char *str) {
    char_array_append(array, str);
    char_array_terminate(array);
}

void char_array_add_len(char_array *array, char *str, size_t len) {
    char_array_append_len(array, str, len);
    char_array_terminate(array);
}


static void vchar_array_append_joined(char_array *array, char *separator, int count, va_list args) {
    if (count <= 0) {
        return;        
    }

    for (size_t i = 0; i < count - 1; i++) {
        char *arg = va_arg(args, char *);
        char_array_append(array, arg);
        char_array_append(array, separator);
    }

    char *arg = va_arg(args, char *);
    char_array_append(array, arg);
    char_array_terminate(array);

}

void char_array_add_joined(char_array *array, char *separator, int count, ...) {
    va_list args;
    va_start(args, count);
    vchar_array_append_joined(array, separator, count, args);
    va_end(args);
}

void char_array_cat_joined(char_array *array, char *separator, int count, ...) {
    char_array_strip_nul_byte(array);
    va_list args;
    va_start(args, count);
    vchar_array_append_joined(array, separator, count, args);
    va_end(args);
}

// Based on antirez's sdscatvprintf implementation
void char_array_cat_printf(char_array *array, char *format, ...) {
    va_list args;
    va_start(args, format);

    char_array_strip_nul_byte(array);

    va_list cpy;

    char *buf;
    size_t buflen;

    size_t last_n = array->n;
    size_t size = array->m < 8 ? 16 : array->m * 2;

    while(1) {
        char_array_resize(array, size);
        buf = array->a + last_n;
        buflen = size - last_n;
        if (buf == NULL) return;
        array->a[size-2] = '\0';
        va_copy(cpy, args);
        vsnprintf(buf, buflen, format, cpy);
        if (array->a[size-2] != '\0') {
            size *= 2;
            continue;
        } else {
            array->n += strlen(buf);
        }
        break;
    }

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
    cstring_array *array = malloc(sizeof(cstring_array));
    if (array == NULL) return NULL;

    array->str = str;
    array->indices = uint32_array_new_size(1);
    uint32_array_push(array->indices, 0);
    char *ptr = str->a;
    uint32_t i = 0;
    for (i = 0; i < str->n - 1; i++, ptr++) {
        if (*ptr == '\0') {
            uint32_array_push(array->indices, i + 1);
        }
    }
    return array;
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

inline uint32_t cstring_array_start_token(cstring_array *self) {
    uint32_t index = self->str->n;
    uint32_array_push(self->indices, index);
    return index;
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

inline int32_t cstring_array_get_offset(cstring_array *self, uint32_t i) {
    if (INVALID_INDEX(i, self->indices->n)) {
        return -1;
    }
    return (int32_t)self->indices->a[i];
}

inline char *cstring_array_get_token(cstring_array *self, uint32_t i) {
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

cstring_array *cstring_array_split(char *str, const char *separator, size_t separator_len, int *count) {
    *count = 0;
    char_array *array = char_array_new_size(strlen(str));

    while (*str) {
        if ((separator_len == 1 && *str == separator[0]) || (memcmp(str, separator, separator_len) == 0)) {
            char_array_push(array, '\0');
            str += separator_len;
        } else {
            char_array_push(array, *str);
            str++;
        }
    }
    char_array_push(array, '\0');

    return cstring_array_from_char_array(array);
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

inline void string_tree_finalize_token(string_tree_t *self) {
    uint32_array_push(self->token_indices, cstring_array_num_strings(self->strings));
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

inline uint32_t string_tree_num_alternatives(string_tree_t *self, uint32_t i) {
    if (i >= self->token_indices->n) return 0;
    return self->token_indices->a[i + 1] - self->token_indices->a[i];
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

#define STRING_TREE_ITER_DIRECTION_LEFT -1
#define STRING_TREE_ITER_DIRECTION_RIGHT 1


string_tree_iterator_t *string_tree_iterator_new(string_tree_t *tree) {
    string_tree_iterator_t *self = malloc(sizeof(string_tree_iterator_t));
    self->tree = tree;

    uint32_t num_tokens = tree->token_indices->n - 1;
    self->num_tokens = num_tokens;

    // calloc since the first path through the tree is all zeros
    self->path = calloc(num_tokens, sizeof(uint32_t));

    self->num_alternatives = calloc(num_tokens, sizeof(uint32_t));

    uint64_t permutations = 1;
    uint32_t num_strings;

    for (int i = 0; i < num_tokens; i++) {
        // N + 1 indices stored in token_indices, so this is always valid
        num_strings = string_tree_num_alternatives(tree, i);
        if (num_strings > 0) {
            self->num_alternatives[i] = num_strings;
            // 1 or more strings in the string_tree means use those instead of the actual token
            permutations *= num_strings;
        }
    }

    if (permutations > 1) {
        self->remaining = permutations;
        self->single_path = false;
    } else{
        self->remaining = 1;
        self->single_path = true;
    }

    // Start on the right going backward
    self->cursor = self->num_tokens - 1;
    self->direction = -1;

    return self;
}

static inline void string_tree_iterator_switch_direction(string_tree_iterator_t *self) {
    self->direction *= -1;
}

static inline void string_tree_iterator_reset_right(string_tree_iterator_t *self) {
    size_t offset = self->cursor + 1;
    size_t num_bytes = self->num_tokens - offset;
    memset(self->path + offset, 0, sizeof(uint32_t) * num_bytes);
}

static int string_tree_iterator_do_iteration(string_tree_iterator_t *self) {
    uint32_t num_alternatives;
    int32_t direction = self->direction;
    string_tree_t *tree = self->tree;

    uint32_t sentinel = (direction == STRING_TREE_ITER_DIRECTION_LEFT ? -1 : self->num_tokens); 

    for (uint32_t cursor = self->cursor; cursor != sentinel; cursor += direction) {
        self->cursor = cursor;
        num_alternatives = self->num_alternatives[cursor];
        if (num_alternatives > 1 && self->path[cursor] < num_alternatives - 1) {
            self->path[cursor]++;
            if (direction == STRING_TREE_ITER_DIRECTION_LEFT && self->cursor < self->num_tokens - 1) {
                string_tree_iterator_reset_right(self);
                self->cursor++;
            }
            string_tree_iterator_switch_direction(self);
            self->remaining--;
            return 1;
        }
    }
    
    return -1;
}

void string_tree_iterator_next(string_tree_iterator_t *self) {
    if (!self->single_path && string_tree_iterator_do_iteration(self) == -1 && self->remaining > 0) {
        string_tree_iterator_switch_direction(self);
        if (string_tree_iterator_do_iteration(self) == -1) {
            self->remaining = 0;
        }
    } else if (self->single_path) {       
        self->remaining--;
        return;
    }
}


char *string_tree_iterator_get_string(string_tree_iterator_t *self, uint32_t i) {
    if (i >= self->num_tokens) {
        return NULL;
    }
    uint32_t base_index = self->tree->token_indices->a[i];
    uint32_t offset = self->path[i];

    return cstring_array_get_token(self->tree->strings, base_index + offset);
}

bool string_tree_iterator_done(string_tree_iterator_t *self) {
    return self->remaining > 0;
}

void string_tree_iterator_destroy(string_tree_iterator_t *self) {
    if (self == NULL) return;

    if (self->path) {
        free(self->path);
    }

    if (self->num_alternatives) {
        free(self->num_alternatives);
    }

    free(self);
}
