#ifndef UNICODE_SCRIPTS_H
#define UNICODE_SCRIPTS_H

#include <stdlib.h>
#include <stdint.h>

#include "constants.h"
#include "string_utils.h"
#include "utf8proc/utf8proc.h"
#include "unicode_script_types.h"

typedef struct script_code {
    script_t script;
    char *code;
} script_code_t;

typedef struct script_language {
    script_t script;
    char language[MAX_LANGUAGE_LEN];
} script_language_t;

typedef struct script_languages {
    size_t num_languages;
    char *languages[MAX_LANGS];
} script_languages_t;

typedef struct string_script {
    script_t script;
    size_t len;
    bool ascii;
} string_script_t;

script_t get_char_script(uint32_t ch);
script_languages_t get_script_languages(script_t script);

string_script_t get_string_script(char *str, size_t len);

#endif
