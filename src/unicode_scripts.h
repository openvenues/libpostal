#ifndef UNICODE_SCRIPTS_H
#define UNICODE_SCRIPTS_H

#include <stdlib.h>
#include "unicode_script_types.h"

typedef struct script_code {
    script_t script;
    char *code;
} script_code_t;

typedef struct script_language {
    size_t num_languages;
    char *languages[MAX_LANGS];
} script_language_t;

script_t get_char_script(uint32_t ch);
script_language_t get_script_languages(script_t script);

#endif
