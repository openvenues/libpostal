#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>

#include "libpostal.h"
#include "log/log.h"
#include "json_encode.h"
#include "string_utils.h"

#define LIBPOSTAL_USAGE "Usage: ./libpostal address language [--json]\n"

int main(int argc, char **argv) {
    uint64_t i;
    char *arg;

    char *address = NULL;
    char *language = NULL;

    bool use_json = false;

    string_array *languages = NULL;

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (string_equals(arg, "-h") || string_equals(arg, "--help")) {
            printf(LIBPOSTAL_USAGE);
            exit(EXIT_SUCCESS);
        } else if (string_equals(arg, "--json")) {
            use_json = true;
        } else if (address == NULL) {
            address = arg;
        } else if (!string_starts_with(arg, "-")) {
            if (languages == NULL) {
                languages = string_array_new();
            }
            string_array_push(languages, arg);
        }
    }

    if (address == NULL || languages == NULL) {
        log_error(LIBPOSTAL_USAGE);
        exit(EXIT_FAILURE);
    }

    if (!libpostal_setup()) {
        exit(EXIT_FAILURE);
    }

    normalize_options_t options = LIBPOSTAL_DEFAULT_OPTIONS;
    options.languages = languages->a;
    options.num_languages = languages->n;

    size_t num_expansions;

    char **strings = expand_address(address, options, &num_expansions);

    string_array_destroy(languages);

    char *normalized;

    if (!use_json) {
        for (i = 0; i < num_expansions; i++) {
            normalized = strings[i];
            printf("%s\n", normalized);        
            free(normalized);
        }
    } else {
        printf("{\"expansions\": [");
        for (i = 0; i < num_expansions; i++) {
            normalized = strings[i];
            char *json_string = json_encode_string(normalized);
            printf("%s%s", json_string, i < num_expansions - 1 ? ", ": "");
            free(normalized);
            free(json_string);
        }
        printf("]}\n");

    }

    free(strings);

    libpostal_teardown();
}
