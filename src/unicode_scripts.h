#ifndef UNICODE_SCRIPTS_H
#define UNICODE_SCRIPTS_H

#include <stdlib.h>
#include "utf8proc/utf8proc.h"
#include "unicode_script_types.h"

typedef struct script_code {
    script_t script;
    char *code;
} script_code_t;

typedef struct script_languages {
    size_t num_languages;
    char *languages[MAX_LANGS];
} script_languages_t;

script_t get_char_script(uint32_t ch);
script_languages_t get_script_languages(script_t script);

script_t string_script(char *str, size_t *len);

#endif
