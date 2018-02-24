#include <stdio.h>
#include <stdlib.h>

#include "libpostal.h"
#include "string_utils.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: ./test_near_dupe label value [...]\n");
        exit(EXIT_FAILURE);
    }

    if (!libpostal_setup() || !libpostal_setup_language_classifier()) {
        exit(EXIT_FAILURE);
    }

    libpostal_near_dupe_hash_options_t options = libpostal_get_near_dupe_hash_default_options();

    cstring_array *labels_array = cstring_array_new();
    cstring_array *values_array = cstring_array_new();
    cstring_array *languages_array = NULL;

    bool label = true;
    bool next_is_latitude = false;
    bool next_is_longitude = false;
    bool next_is_geohash_precision = false;
    bool have_latitude = false;
    bool have_longitude = false;
    bool next_is_language = false;
    double longitude = 0.0;
    double latitude = 0.0;

    for (size_t i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (string_equals(arg, "--with-unit")) {
            options.with_unit = true;
        } else if (string_equals(arg, "--latitude")) {
            next_is_latitude = true;
        } else if (string_equals(arg, "--longitude")) {
            next_is_longitude = true;
        } else if (string_equals(arg, "--geohash-precision")) {
            next_is_geohash_precision = true;
        } else if (string_equals(arg, "--name-only-keys")) {
            options.name_only_keys = true;
        } else if (string_equals(arg, "--address-only-keys")) {
            options.address_only_keys = true;
        } else if (string_equals(arg, "--name-only-keys")) {
            options.name_only_keys = true;
        } else if (string_equals(arg, "--use-city")) {
            options.with_city_or_equivalent = true;
        } else if (string_equals(arg, "--use-containing")) {
            options.with_small_containing_boundaries = true;
        } else if (string_equals(arg, "--use-postal-code")) {
            options.with_postal_code = true;
        } else if (string_equals(arg, "--language")) {
            next_is_language = true;
        } else if (next_is_latitude) {
            sscanf(arg, "%lf", &latitude);
            next_is_latitude = false;
            have_latitude = true;
        } else if (next_is_longitude) {
            sscanf(arg, "%lf", &longitude);
            next_is_longitude = false;
            have_longitude = true;
        } else if (next_is_geohash_precision) {
            size_t geohash_precision = 0;
            sscanf(arg, "%zu", &geohash_precision);
            options.geohash_precision = geohash_precision;
            next_is_geohash_precision = false;
        } else if (next_is_language) {
            if (languages_array == NULL) {
                languages_array = cstring_array_new();
            }
            cstring_array_add_string(languages_array, arg);
        } else if (label) {
            cstring_array_add_string(labels_array, arg);
            label = false;
        } else {
            cstring_array_add_string(values_array, arg);
            label = true;
        }
    }

    if (have_latitude && have_longitude) {
        options.with_latlon = true;
        options.latitude = latitude;
        options.longitude = longitude;
    }

    size_t num_languages = 0;
    char **languages = NULL;
    if (languages_array != NULL) {
        num_languages = cstring_array_num_strings(languages_array);
        languages = cstring_array_to_strings(languages_array);
    }


    size_t num_components = cstring_array_num_strings(labels_array);
    if (num_components != cstring_array_num_strings(values_array)) {
        cstring_array_destroy(labels_array);
        cstring_array_destroy(values_array);
        printf("Must have same number of labels and values\n");
        exit(EXIT_FAILURE);
    }

    char **labels = cstring_array_to_strings(labels_array);
    char **values = cstring_array_to_strings(values_array);

    size_t num_near_dupe_hashes = 0;
    char **near_dupe_hashes = libpostal_near_dupe_hashes_languages(num_components, labels, values, options, num_languages, languages, &num_near_dupe_hashes);
    if (near_dupe_hashes != NULL) {
        for (size_t i = 0; i < num_near_dupe_hashes; i++) {
            char *near_dupe_hash = near_dupe_hashes[i];
            printf("%s\n", near_dupe_hash);
        }

        libpostal_expansion_array_destroy(near_dupe_hashes, num_near_dupe_hashes);
    }

    libpostal_expansion_array_destroy(labels, num_components);
    libpostal_expansion_array_destroy(values, num_components);

    if (languages != NULL) {
        libpostal_expansion_array_destroy(languages, num_languages);
    }

    libpostal_teardown();
    libpostal_teardown_language_classifier();

}
