#include "features.h"


void feature_array_add(cstring_array *features, size_t count, ...) {
    if (count <= 0) {
        return;        
    }

    va_list args;
    va_start(args, count);

    cstring_array_start_token(features);

    for (size_t i = 0; i < count - 1; i++) {
        char *arg = va_arg(args, char *);
        char_array_append(features->str, arg);
        char_array_append(features->str, FEATURE_SEPARATOR_CHAR);
    }

    char *arg = va_arg(args, char *);
    char_array_append(features->str, arg);
    char_array_terminate(features->str);

    va_end(args);
}