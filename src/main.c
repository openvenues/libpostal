#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "libpostal.h"
#include "log/log.h"
#include "string_utils.h"

#define LIBPOSTAL_USAGE "Usage: ./libpostal address [language ...]\n"

int main(int argc, char **argv) {
    int i;
    char *arg;

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (string_equals(arg, "-h") || string_equals(arg, "--help")) {
            printf(LIBPOSTAL_USAGE);
            exit(EXIT_SUCCESS);
        }
    }

    if (argc < 3) {
        log_error(LIBPOSTAL_USAGE);
        exit(EXIT_FAILURE);
    }

    char *str = argv[1];
    char *languages[argc - 2];
    for (i = 0; i < argc - 2; i++) {
        arg = argv[i + 2];
        if (strlen(arg) >= MAX_LANGUAGE_LEN) {
            printf("arg %d was longer than a language code (%d chars). Make sure to quote the input string\n", i + 2, MAX_LANGUAGE_LEN - 1);
        }
        languages[i] = arg;
    }

    if (!libpostal_setup()) {
        exit(EXIT_FAILURE);
    }

    normalize_options_t options = LIBPOSTAL_DEFAULT_OPTIONS;
    options.languages = languages;

    size_t num_expansions;

    char **strings = expand_address(str, options, &num_expansions);

    char *normalized;
    for (uint64_t i = 0; i < num_expansions; i++) {
        normalized = strings[i];
        printf("%s\n", normalized);        
        free(normalized);
    }

    free(strings);

    libpostal_teardown();
}
