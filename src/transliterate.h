#ifndef TRANSLITERATE_H
#define TRANSLITERATE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "collections.h"
#include "constants.h"
#include "klib/khash.h"
#include "string_utils.h"
#include "trie.h"
#include "trie_search.h"
#include "unicode_scripts.h"

#define LATIN_ASCII "latin-ascii"
#define LATIN_ASCII_SIMPLE "latin-ascii-simple"
#define HTML_ESCAPE "html-escape"

#define TRANSLITERATION_DATA_FILE "transliteration.dat"
#define DEFAULT_TRANSLITERATION_PATH LIBPOSTAL_TRANSLITERATION_DIR PATH_SEPARATOR TRANSLITERATION_DATA_FILE

#define MAX_TRANS_NAME_LEN 100

typedef enum {
    STEP_RULESET,
    STEP_TRANSFORM,
    STEP_UNICODE_NORMALIZATION
} step_type_t;

typedef struct transliteration_step {
    step_type_t type;
    char *name;
} transliteration_step_t;

transliteration_step_t *transliteration_step_new(char *name, step_type_t type);
void transliteration_step_destroy(transliteration_step_t *self);

VECTOR_INIT_FREE_DATA(step_array, transliteration_step_t *, transliteration_step_destroy)

typedef struct transliterator {
    char *name;
    uint8_t internal;
    uint32_t steps_index;
    size_t steps_length;
} transliterator_t;

#define MAX_GROUP_LEN 5

typedef struct group_capture {
    size_t start;
    size_t len;
} group_capture_t;

VECTOR_INIT(group_capture_array, group_capture_t)

typedef struct transliteration_replacement {
    uint32_t string_index;
    uint32_t revisit_index;
    size_t num_groups;
    group_capture_array *groups;
} transliteration_replacement_t;

transliteration_replacement_t *transliteration_replacement_new(
    uint32_t string_index,
    uint32_t revisit_index,
    group_capture_array *groups
);

void transliteration_replacement_destroy(transliteration_replacement_t *self);

VECTOR_INIT_FREE_DATA(transliteration_replacement_array, transliteration_replacement_t *, transliteration_replacement_destroy)

KHASH_MAP_INIT_STR(str_transliterator, transliterator_t *)

#define kh_script_lang_hash(key)  ((khint_t)(key).script ^ (((key).language == NULL) ? 0 : kh_str_hash_func((key).language)))
#define kh_script_lang_equal(a, b)  (((a).script == (b).script) && strcmp((a).language, (b).language) == 0)

typedef struct transliterator_index {
    size_t transliterator_index;
    size_t num_transliterators;
} transliterator_index_t;

#define NULL_TRANSLITERATOR_INDEX (transliterator_index_t) {0, 0}

KHASH_INIT(script_language_index, script_language_t, transliterator_index_t, 1, kh_script_lang_hash, kh_script_lang_equal)

typedef struct transliteration_table {
    khash_t(str_transliterator) *transliterators;

    khash_t(script_language_index) *script_languages;
    cstring_array *transliterator_names;

    step_array *steps;
    trie_t *trie;

    transliteration_replacement_array *replacements;
    cstring_array *replacement_strings;
    cstring_array *revisit_strings;
} transliteration_table_t;

// Control characters are special
#define WORD_BOUNDARY_CHAR "\x01"
#define WORD_BOUNDARY_CODEPOINT 1
#define WORD_BOUNDARY_CHAR_LEN strlen(WORD_BOUNDARY_CHAR)
#define PRE_CONTEXT_CHAR "\x86"
#define PRE_CONTEXT_CODEPOINT 134
#define PRE_CONTEXT_CHAR_LEN strlen(PRE_CONTEXT_CHAR)
#define POST_CONTEXT_CHAR "\x87"
#define POST_CONTEXT_CODEPOINT 135
#define POST_CONTEXT_CHAR_LEN strlen(POST_CONTEXT_CHAR)
#define EMPTY_TRANSITION_CHAR "\x04"
#define EMPTY_TRANSITION_CODEPOINT 4
#define EMPTY_TRANSITION_CHAR_LEN strlen(EMPTY_TRANSITION_CHAR)
#define REPEAT_CHAR "\x05"
#define REPEAT_CODEPOINT 5
#define REPEAT_CHAR_LEN strlen(REPEAT_CHAR)
#define GROUP_INDICATOR_CHAR "\x1d"
#define GROUP_INDICATOR_CODEPOINT 29
#define GROUP_INDICATOR_CHAR_LEN strlen(GROUP_INDICATOR_CHAR)
#define BEGIN_SET_CHAR "\x0f"
#define BEGIN_SET_CODEPOINT 15
#define BEGIN_SET_CHAR_LEN strlen(BEGIN_SET_CHAR)
#define END_SET_CHAR "\x0e"
#define END_SET_CODEPOINT 14
#define END_SET_CHAR_LEN strlen(END_SET_CHAR)


#define DOLLAR_CODEPOINT 36

#define LPAREN_CODEPOINT 40
#define RPAREN_CODEPOINT 41

#define STAR_CODEPOINT 42
#define PLUS_CODEPOINT 43

#define LSQUARE_CODEPOINT 91
#define BACKSLASH_CODEPOINT 92
#define RSQUARE_CODEPOINT 93

#define LCURLY_CODEPOINT 123
#define RCURLY_CODEPOINT 125


// Primary API
transliteration_table_t *get_transliteration_table(void);

transliterator_t *transliterator_new(char *name, uint8_t internal, uint32_t steps_index, size_t steps_length);
void transliterator_destroy(transliterator_t *self);

bool transliteration_table_add_transliterator(transliterator_t *trans);

transliterator_t *get_transliterator(char *name);
char *transliterate(char *trans_name, char *str, size_t len);

bool transliteration_table_add_script_language(script_language_t script_language, transliterator_index_t index);
transliterator_index_t get_transliterator_index_for_script_language(script_t script, char *language);

#define foreach_transliterator(script, language, transliterator_var, code) do {                                                     \
        transliteration_table_t *__trans_table = get_transliteration_table();                                                       \
        transliterator_index_t __index = get_transliterator_index_for_script_language(script, language);                            \
        for (size_t __i = __index.transliterator_index; __i < __index.transliterator_index + __index.num_transliterators; __i++) {  \
            transliterator_var = cstring_array_get_string(__trans_table->transliterator_names, (uint32_t)__i);                                \
            if (transliterator_var == NULL) break;                                                                                  \
            code;                                                                                                                   \
        }                                                                                                                           \
    } while (0);

bool transliteration_table_write(FILE *file);
bool transliteration_table_save(char *filename);

// Module setup/teardown
bool transliteration_module_init(void);
bool transliteration_module_setup(char *filename);
void transliteration_module_teardown(void);

#endif
