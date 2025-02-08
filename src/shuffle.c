#include "shuffle.h"

#include <config.h>

#include "string_utils.h"

#include "file_utils.h"

// Run shuf/gshuf on a file in-place if the shuf command is available.
bool shuffle_file(char *filename) {
    char *shuffle_command = NULL;

    #if defined(HAVE_SHUF)
    shuffle_command = "shuf";
    #elif defined(HAVE_GSHUF)
    shuffle_command = "gshuf";
    #else
    return false;
    #endif

    char_array *command = char_array_new();

    char_array_cat_printf(command, "%s -o %s %s", shuffle_command, filename, filename);

    int ret = system(char_array_get_string(command));

    char_array_destroy(command);

    return ret == EXIT_SUCCESS;
}

// Assign each line of the file randomly to n chunks and shuffle each file sequentially in-memory
// This approach will produce a random permutation of the lines using limited memory
bool shuffle_file_chunked(char *filename, size_t parts) {
    char *shuffle_command = NULL;

    // Linux
    #if defined(HAVE_SHUF)
    shuffle_command = "shuf";
    // Mac
    #elif defined(HAVE_GSHUF)
    shuffle_command = "gshuf";
    #else
    return false;
    #endif

    if (filename == NULL) {
        return false;
    }

    // Make sure the input file exists
    if (!file_exists(filename)) {
        return false;
    }

    // This is an in-place shuffle to keep the API simple
    char *outfile = filename;

    char_array *command = char_array_new();
    if (command == NULL) {
        return false;
    }

    // Split the file randomly into $parts files
    // Need to be assigned randomly, not just every nth line or it's not really a random permutation
    char_array_cat_printf(command, "awk -v parts=%zu 'BEGIN{srand();} { f = \"%s.\"int(rand() * parts); print > f }' %s", parts, filename, filename);

    int ret = system(char_array_get_string(command));
    if (ret != EXIT_SUCCESS) {
        goto exit_char_array_allocated;
    }

    // Run shuf sequentially on each of the $parts files
    // This should be sequential, not parallelized as the goal is
    // to limit memory usage when shuffling large files
    for (size_t i = 0; i < parts; i++) {
        char_array_clear(command);
        char_array_cat_printf(command, "%s %s.%zu %s %s.tmp", shuffle_command, filename, i, i > 0 ? ">>" : ">", outfile);
        ret = system(char_array_get_string(command));
        if (ret != EXIT_SUCCESS) {
            goto exit_char_array_allocated;
        }

        // Delete the file temp file
        char_array_clear(command);
        char_array_cat_printf(command, "rm %s.%zu", filename, i);
        ret = system(char_array_get_string(command));
        if (ret != EXIT_SUCCESS) {
            goto exit_char_array_allocated;
        }
    }

    char_array_clear(command);
    char_array_cat_printf(command, "mv %s.tmp %s", outfile, outfile);
    ret = system(char_array_get_string(command));
    if (ret != EXIT_SUCCESS) {
        goto exit_char_array_allocated;
    }


exit_char_array_allocated:
    char_array_destroy(command);
    return ret == EXIT_SUCCESS;
}

// Shuffle a file in-place, specifying a rough upper bound on system memory
bool shuffle_file_chunked_size(char *filename, size_t chunk_size) {
    FILE *f = fopen(filename, "r");

    if (f == NULL) return false;

    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
    fclose(f);

    size_t parts = size / chunk_size + 1;
    // If the file is smaller than the chunk size, do a
    // simple in-memory shuffle of the whole file
    if (parts == 1) {
        return shuffle_file(filename);
    }

    return shuffle_file_chunked(filename, parts);
}


