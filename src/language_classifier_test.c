#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"
#include "address_dictionary.h"
#include "language_classifier.h"
#include "language_classifier_io.h"
#include "string_utils.h"
#include "trie_utils.h"
#include "libpostal.h"


double test_accuracy(libpostal_t *instance, char *filename) {
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

    while (language_classifier_data_set_next(instance, data_set)) {
        char *address = char_array_get_string(data_set->address);
        char *language = char_array_get_string(data_set->language);
        
        uint32_t label_id;
        if (!trie_get_data(label_ids, language, &label_id)) {
            continue;
        }

        language_classifier_response_t *response = classify_languages(instance, address);
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

    transliteration_table_t *trans_table = transliteration_module_setup(NULL);
    numex_table_t *numex_table = numex_module_setup(NULL);
    address_dictionary_t *address_dict = address_dictionary_module_setup(NULL);

    if (trans_table == NULL || numex_table == NULL || address_dict == NULL || !language_classifier_module_setup(dir)) {
        log_error("Error setting up classifier\n");
    }

    libpostal_t instance = { 0 };
    instance.trans_table = trans_table;
    instance.numex_table = numex_table;
    instance.address_dict = address_dict;

    double accuracy = test_accuracy(&instance, filename);
    log_info("Done. Accuracy: %f\n", accuracy);
}
