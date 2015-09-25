#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "libpostal.h"
#include "log/log.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        log_error("Usage: test_libpostal string languages...\n");
        exit(EXIT_FAILURE);
    }
    char *str = argv[1];
    char *languages[argc - 2];
    for (int i = 0; i < argc - 2; i++) {
        char *arg = argv[i + 2];
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

    uint64_t num_expansions;

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
