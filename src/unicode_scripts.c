#include "unicode_scripts.h"

#include "unicode_scripts_data.c"

inline script_t get_char_script(uint32_t ch) {
    if (ch > NUM_CODEPOINTS - 1) return SCRIPT_UNKNOWN;
    return char_scripts[ch];
}

inline script_languages_t get_script_languages(script_t script) {
    return script_languages[script];
}

script_t string_script(char *str, size_t *len) {
    int32_t ch;
    script_t last_script = SCRIPT_UNKNOWN;
    script_t script = SCRIPT_UNKNOWN;

    uint8_t *ptr = (uint8_t *)str;

    *len = 0;

    while (1) {
        ssize_t char_len = utf8proc_iterate(ptr, -1, &ch);

        if (ch == 0) break;

        script = get_char_script((uint32_t)ch);

        if (script == SCRIPT_COMMON) {
            script = last_script;
        }

        if (last_script != script && last_script != SCRIPT_UNKNOWN) {
            break;
        }

        ptr += char_len;
        *len += char_len;

        if (script != SCRIPT_UNKNOWN || script != SCRIPT_COMMON) {
            last_script = script;
        }
    
    }

    return last_script;
}