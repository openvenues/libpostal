#include "unicode_scripts.h"

#include "unicode_scripts_data.c"

#define MAX_ASCII 128

inline script_t get_char_script(uint32_t ch) {
    if (ch > NUM_CODEPOINTS - 1) return SCRIPT_UNKNOWN;
    return char_scripts[ch];
}

inline script_languages_t get_script_languages(script_t script) {
    return script_languages[script];
}

static inline bool is_common_script(script_t script) {
    return script == SCRIPT_COMMON || script == SCRIPT_INHERITED;
}

string_script_t get_string_script(char *str, size_t len) {
    int32_t ch;
    script_t last_script = SCRIPT_UNKNOWN;
    script_t script = SCRIPT_UNKNOWN;

    uint8_t *ptr = (uint8_t *)str;

    size_t script_len = 0;
    size_t idx = 0;

    bool is_ascii = true;

    while (idx < len) {
        ssize_t char_len = utf8proc_iterate(ptr, len, &ch);

        if (ch == 0) break;

        script = get_char_script((uint32_t)ch);

        if (is_common_script(script) && last_script != SCRIPT_UNKNOWN) {
            script = last_script;
        }

        if (last_script != script && last_script != SCRIPT_UNKNOWN && !is_common_script(last_script)) {
            if (script_len < len) {
                while (true) {
                    char_len = utf8proc_iterate_reversed((const uint8_t *)str, idx, &ch);
                    if (ch == 0) break;

                    script = get_char_script((uint32_t)ch);
                    if (!is_common_script(script)) {
                        break;
                    }

                    script_len -= char_len;
                    ptr -= char_len;
                    idx -= char_len;
                }
            }

            break;
        }

        is_ascii = is_ascii && ch < MAX_ASCII;

        ptr += char_len;
        idx += char_len;
        script_len += char_len;

        if (script != SCRIPT_UNKNOWN) {
            last_script = script;
        }
    
    }

    return (string_script_t) {last_script, script_len, is_ascii};
}