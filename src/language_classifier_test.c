#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"
#include "address_dictionary.h"
#include "language_classifier.h"
#include "language_classifier_io.h"
#include "string_utils.h"
#include "trie_utils.h"
#include "transliterate.h"


double test_accuracy(char *filename) {
    language_classifier_data_set_t *data_set = language_classifier_data_set_init(filename);
    if (data_set == NULL) {
        log_error("Error creating data set\n");
        exit(EXIT_FAILURE);
    }

    language_classifier_minibatch_t *minibatch;

    uint32_t correct = 0;
    uint32_t total = 0;

    language_classifier_t *classifier = get_language_classifier();
    trie_t *label_ids = trie_new_from_cstring_array(classifier->labels);

    while (language_classifier_data_set_next(data_set)) {
        char *address = char_array_get_string(data_set->address);
        char *language = char_array_get_string(data_set->language);
        
        uint32_t label_id;
        if (!trie_get_data(label_ids, language, &label_id)) {
            continue;
        }

        language_classifier_response_t *response = classify_languages(address);
        if (response == NULL || response->num_languages == 0) {
            printf("%s\tNULL\t%s\n", language, address);
            continue;
        }

        char *top_lang = response->languages[0];

        if (string_equals(top_lang, language)) {
            correct++;
        } else {
            printf("%s\t%s\t%s\n", language, top_lang, address);
        }

        total++;

        language_classifier_response_destroy(response);

    }

    log_info("total=%" PRIu32 "\n", total);

    trie_destroy(label_ids);

    return (double) correct / total;


}


int main(int argc, char **argv) {
    if (argc < 2) {
        log_error("Usage: language_classifier_test [dir] filename\n");
        exit(EXIT_FAILURE);
    }

    char *dir = LIBPOSTAL_LANGUAGE_CLASSIFIER_DIR;
    char *filename = NULL;

    if (argc >= 3) {
        dir = argv[1];
        filename = argv[2];
    } else {
        filename = argv[1];
    }

    if (!language_classifier_module_setup(dir) || !address_dictionary_module_setup(NULL) || !transliteration_module_setup(NULL)) {
        log_error("Error setting up classifier\n");
    }

    double accuracy = test_accuracy(filename);
    log_info("Done. Accuracy: %f\n", accuracy);
}
