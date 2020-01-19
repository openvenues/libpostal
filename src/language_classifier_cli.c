#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "address_dictionary.h"
#include "language_classifier.h"
#include "transliterate.h"
#include "libpostal.h"


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./language_classifier [dir] address\n");
        exit(EXIT_FAILURE);
    }

    char *dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;
    char *address = NULL;

    if (argc >= 3) {
        dir = argv[1];
        address = strdup(argv[2]);
    } else {
        address = strdup(argv[1]);
    }

    address_dictionary_t *address_dict = address_dictionary_module_setup(NULL);
    transliteration_table_t *trans_table = transliteration_module_setup(NULL);

    libpostal_t instance = { 0 };
    instance.address_dict = address_dict;
    instance.trans_table = trans_table;

    if (address_dict == NULL || trans_table == NULL || !language_classifier_module_setup(dir)) {
        log_error("Could not load language classifiers\n");
        exit(EXIT_FAILURE);
    }


    language_classifier_response_t *response = classify_languages(&instance, address);
    if (response == NULL) {
        printf("Could not classify language\n");
        exit(EXIT_FAILURE);
    }

    printf("Languages:\n\n");

    for (size_t i = 0; i < response->num_languages; i++) {
        printf("%s (%f)\n", response->languages[i], response->probs[i]);
    }

    free(address);
    language_classifier_response_destroy(response);

    language_classifier_module_teardown();
    address_dictionary_module_teardown(&address_dict);
}
