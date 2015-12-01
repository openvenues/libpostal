#include "shuffle.h"

#include <config.h>
#include "string_utils.h"

bool shuffle_file(char *filename) {
    char *shuffle_command = NULL;

    #if defined(HAVE_SHUF)
    shuffle_command = "shuf";
    #else
    return false;
    #endif

    char_array *command = char_array_new();

    char_array_cat_printf(command, "%s -o %s %s", shuffle_command, filename, filename);

    int ret = system(char_array_get_string(command));

    char_array_destroy(command);

    return ret == EXIT_SUCCESS;
}
