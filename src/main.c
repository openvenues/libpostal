#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "libpostal.h"
#include "file_utils.h"
#include "log/log.h"
#include "json_encode.h"
#include "string_utils.h"

#define LIBPOSTAL_USAGE "Usage: ./libpostal address [...languages] [--json]\n"

static inline void print_output(char *address, libpostal_normalize_options_t options, bool use_json, bool root_expansions) {
    size_t num_expansions;

    char **expansions;

    if (!root_expansions) {
        expansions = libpostal_expand_address(address, options, &num_expansions);
    } else {
        expansions = libpostal_expand_address_root(address, options, &num_expansions);
    }

    char *normalized;

    if (!use_json) {
        for (size_t i = 0; i < num_expansions; i++) {
            normalized = expansions[i];
            printf("%s\n", normalized);        
        }
    } else {
        printf("{\"expansions\": [");
        for (size_t i = 0; i < num_expansions; i++) {
            normalized = expansions[i];
            char *json_string = json_encode_string(normalized);
            printf("%s%s", json_string, i < num_expansions - 1 ? ", ": "");
            free(json_string);
        }
        printf("]}\n");

    }

    libpostal_expansion_array_destroy(expansions, num_expansions);

}

int main(int argc, char **argv) {
    char *arg;

    char *address = NULL;

    bool use_json = false;
    bool root_expansions = false;

    string_array *languages = NULL;

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (string_equals(arg, "-h") || string_equals(arg, "--help")) {
            printf(LIBPOSTAL_USAGE);
            exit(EXIT_SUCCESS);
        } else if (string_equals(arg, "--json")) {
            use_json = true;
        } else if (string_equals(arg, "--root")) {
            root_expansions = true;
        } else if (address == NULL) {
            address = arg;
        } else if (!string_starts_with(arg, "-")) {
            if (languages == NULL) {
                languages = string_array_new();
            }
            string_array_push(languages, arg);
        }
    }

    if (address == NULL && (!use_json || isatty(fileno(stdin)))) {
        log_error(LIBPOSTAL_USAGE);
        exit(EXIT_FAILURE);
    }

    if (!libpostal_setup() || (languages == NULL && !libpostal_setup_language_classifier())) {
        exit(EXIT_FAILURE);
    }

    libpostal_normalize_options_t options = libpostal_get_default_options();

    if (languages != NULL) {
        options.languages = languages->a;
        options.num_languages = languages->n;
    }

    if (address == NULL) {
        char *line;
        while ((line = file_getline(stdin)) != NULL) {
            print_output(line, options, use_json, root_expansions);
            free(line);
        }
    } else {
        print_output(address, options, use_json, root_expansions);
    }

    if (languages != NULL) {
        string_array_destroy(languages);
    }

    libpostal_teardown();
    libpostal_teardown_language_classifier();
}
