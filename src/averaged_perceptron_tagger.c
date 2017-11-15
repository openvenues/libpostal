#include "averaged_perceptron_tagger.h"
#include "log/log.h"

bool averaged_perceptron_tagger_predict(averaged_perceptron_t *model, void *tagger, void *context, cstring_array *features, cstring_array *prev_tag_features, cstring_array *prev2_tag_features, cstring_array *labels, tagger_feature_function feature_function, tokenized_string_t *tokenized, bool print_features) {

    // Keep two tags of history in training
    char *prev = NULL;
    char *prev2 = NULL;

    uint32_t prev_id = 0;
    uint32_t prev2_id = 0;

    size_t num_tokens = tokenized->tokens->n;

    for (uint32_t i = 0; i < num_tokens; i++) {
        cstring_array_clear(features);
        cstring_array_clear(prev_tag_features);
        cstring_array_clear(prev2_tag_features);

        if (i > 0) {
            prev = cstring_array_get_string(model->classes, prev_id);
        }

        if (i > 1) {
            prev2 = cstring_array_get_string(model->classes, prev2_id);            
        }

        log_debug("prev=%s, prev2=%s\n", prev, prev2);

        if (!feature_function(tagger, context, tokenized, i)) {
            log_error("Could not add address parser features\n");
            return false;
        }

        uint32_t fidx;
        const char *feature;

        if (prev) {
            cstring_array_foreach(prev_tag_features, fidx, feature, {
                feature_array_add(features, 3, "prev", prev, (char *)feature);
            })
        }

        if (prev2) {
            cstring_array_foreach(prev2_tag_features, fidx, feature, {
                feature_array_add(features, 5, "prev2", prev2, "prev", prev, (char *)feature);
            })
        }

        if (print_features) {
            printf("{ ");
            size_t num_features = cstring_array_num_strings(features);
            cstring_array_foreach(features, fidx, feature, {
                printf("%s", feature);
                if (fidx < num_features - 1) printf(", ");
            })
            printf(" }\n");
        }


        uint32_t guess = averaged_perceptron_predict(model, features);
        char *predicted = cstring_array_get_string(model->classes, guess);

        cstring_array_add_string(labels, predicted);

        prev2_id = prev_id;
        prev_id = guess;

    }

    return true;

}
