#include "json_encode.h"

char *json_encode_string(char *str) {
    char *ptr = str;
    char_array *json_encoded = char_array_new_size(strlen(str) + 2);
    char_array_push(json_encoded, '"');

    while (*ptr) {
        char ch = *ptr;
        switch (ch) {
            case '\\':
                char_array_append(json_encoded, "\\\\");
                break;
            case '"':
                char_array_append(json_encoded, "\\\"");
                break;
            case '\n':
                char_array_append(json_encoded, "\\n");
                break;
            case '\r':
                char_array_append(json_encoded, "\\r");
                break;
            case '\t':
                char_array_append(json_encoded, "\\t");
                break;
            case '\b':
                char_array_append(json_encoded, "\\\b");
                break;
            case '\f':
                char_array_append(json_encoded, "\\\f");
                break;
            default:
                char_array_push(json_encoded, ch);
        }
        ptr++;
    }
    char_array_push(json_encoded, '"');
    char_array_terminate(json_encoded);

    return char_array_to_string(json_encoded);
}