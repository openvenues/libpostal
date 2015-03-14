#include "features.h"

void feature_array_add(char_array *features, int num_args, ...) {
    if (num_args <= 0) {
        return;        
    }

    va_list args;
    va_start(args, num_args);

    for (int i = 0; i < num_args - 1; i++) {
        char *arg = va_arg(args, char *);
        contiguous_string_array_add_string_unterminated(features, arg);
        contiguous_string_array_add_string_unterminated(features, FEATURE_SEPARATOR_CHAR);
    }

    char *arg = va_arg(args, char *);
    contiguous_string_array_add_string(features, arg);

    va_end(args);

}