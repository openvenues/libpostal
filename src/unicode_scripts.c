#include "unicode_scripts.h"

#include "unicode_scripts_data.c"

script_t get_char_script(uint32_t ch) {
    if (ch > NUM_CHARS - 1) return SCRIPT_UNKNOWN;
    return char_scripts[ch];
}

script_language_t get_script_languages(script_t script) {
    return script_languages[script];
}