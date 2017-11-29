#include <stdio.h>
#include <stdlib.h>

#include "json_encode.h"
#include "libpostal.h"

#include "linenoise/linenoise.h"
#include "log/log.h"
#include "strndup.h"

int main(int argc, char **argv) {
    char *address_parser_dir = NULL;
    char *history_file = "address_parser.history";

    if (argc > 1) {
        address_parser_dir = argv[1];
    }

    printf("Loading models...\n");

    if (!libpostal_setup() || !libpostal_setup_parser_datadir(address_parser_dir)) {
        exit(EXIT_FAILURE);
    }

    printf("\n");

    printf("Welcome to libpostal's address parser.\n\n");
    printf("Type in any address to parse and print the result.\n\n");
    printf("Special commands:\n");
    printf(".exit to quit the program\n\n");

    char *language = NULL;
    char *country = NULL;

    char *input = NULL;

    while((input = linenoise("> ")) != NULL) {

        if (input[0] != '\0') {
            linenoiseHistoryAdd(input); /* Add to the history. */
            linenoiseHistorySave(history_file); /* Save the history on disk. */
        }

        if (strncmp(input, ".exit", 5) == 0) {
            printf("Fin!\n");
            free(input);
            break;
        } else if (strncmp(input, ".language", 9) == 0) {
            size_t num_tokens = 0;
            cstring_array *command = cstring_array_split(input, " ", 1, &num_tokens);
            if (num_tokens > 1) {
                if (language != NULL) {
                    free(language);
                }
                language = strdup(cstring_array_get_string(command, 1));
            } else {
                printf("Must specify language code\n");
            }

            cstring_array_destroy(command);
            goto next_input;
        } else if (strncmp(input, ".country", 8) == 0) {
            size_t num_tokens = 0;
            cstring_array *command = cstring_array_split(input, " ", 1, &num_tokens);
            if (cstring_array_num_strings(command) > 1) {
                if (country != NULL) {
                    free(country);
                }
                country = strdup(cstring_array_get_string(command, 1));
            } else {
                printf("Must specify country code\n");
            }

            cstring_array_destroy(command);
            goto next_input;
        } else if (string_starts_with(input, ".print_features")) {
            size_t num_tokens = 0;
            cstring_array *command = cstring_array_split(input, " ", 1, &num_tokens);
            if (cstring_array_num_strings(command) > 1) {
                char *flag = cstring_array_get_string(command, 1);
                if (string_compare_case_insensitive(flag, "off") == 0) {
                    libpostal_parser_print_features(false);
                } else if (string_compare_case_insensitive(flag, "on") == 0) {
                    libpostal_parser_print_features(true);
                }
            } else {
                libpostal_parser_print_features(true);
            }

            cstring_array_destroy(command);
            goto next_input;
        } else if (strlen(input) == 0) {
            goto next_input;
        }

        libpostal_address_parser_response_t *parsed;
        libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

        if (country != NULL) options.country = country;
        if (language != NULL) options.language = language;

        if ((parsed = libpostal_parse_address(input, options))) {
            printf("\n");
            printf("Result:\n\n");
            printf("{\n");
            for (int i = 0; i < parsed->num_components; i++) {
                char *component = parsed->components[i];

                char *json_string = json_encode_string(component);
                printf("  \"%s\": %s%s\n", parsed->labels[i], json_string, i < parsed->num_components - 1 ? "," : "");
                free(json_string);
            }
            printf("}\n");
            printf("\n");

            libpostal_address_parser_response_destroy(parsed);
        } else {
            log_error("Error parsing address\n");
            exit(EXIT_FAILURE);
        }

next_input:
        free(input);
    }

    libpostal_teardown();
    libpostal_teardown_parser();
}
