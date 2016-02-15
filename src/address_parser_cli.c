#include <stdio.h>
#include <stdlib.h>

#include "address_parser.h"
#include "averaged_perceptron_tagger.h"
#include "address_dictionary.h"
#include "collections.h"
#include "constants.h"
#include "file_utils.h"
#include "geodb.h"
#include "json_encode.h"
#include "libpostal.h"
#include "normalize.h"
#include "scanner.h"
#include "shuffle.h"
#include "tokens.h"

#include "linenoise/linenoise.h"
#include "log/log.h"


bool load_address_parser_dependencies(void) {
    if (!address_dictionary_module_setup(NULL)) {
        log_error("Could not load address dictionaries\n");
        return false;
    }

    log_info("address dictionary module loaded\n");

    if (!geodb_module_setup(NULL)) {
        log_error("Could not load geodb dictionaries\n");
        return false;
    }

    log_info("geodb module loaded\n");

    return true;
}

int main(int argc, char **argv) {
    char *address_parser_dir = LIBPOSTAL_ADDRESS_PARSER_DIR;
    char *history_file = "address_parser.history";

    if (argc > 1) {
        address_parser_dir = argv[1];
    }

    printf("Loading models...\n");

    if (!libpostal_setup() || !libpostal_setup_parser()) {
        exit(EXIT_FAILURE);
    }

    printf("\n");

    printf("Welcome to libpostal's address parser.\n\n");
    printf("Type in any address to parse and print the result.\n\n");
    printf("Special commands:\n\n");
    printf(".language [code] to specify a language\n");
    printf(".country [code] to specify a country\n");
    printf(".exit to quit the program\n\n");

    char *language = NULL;
    char *country = NULL;

    char *input = NULL;

    while((input = linenoise("> ")) != NULL) {

        if (input[0] != '\0') {
            linenoiseHistoryAdd(input); /* Add to the history. */
            linenoiseHistorySave(history_file); /* Save the history on disk. */
        }

        if (strcmp(input, ".exit") == 0) {
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
        } else if (strlen(input) == 0) {
            goto next_input;
        }

        address_parser_response_t *parsed;
        address_parser_options_t options = get_libpostal_address_parser_default_options();

        if ((parsed = parse_address(input, options))) {
            printf("\n");
            printf("Result:\n\n");
            printf("{\n");
            for (int i = 0; i < parsed->num_components; i++) {
                char *json_string = json_encode_string(parsed->components[i]);
                printf("  \"%s\": %s%s\n", parsed->labels[i], json_string, i < parsed->num_components - 1 ? "," : "");
            }
            printf("}\n");
            printf("\n");

            address_parser_response_destroy(parsed);
        } else {
            printf("Error parsing address\n");
        }

next_input:
        free(input);
    }

    libpostal_teardown();
    libpostal_teardown_parser();
}
