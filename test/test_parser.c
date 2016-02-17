#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/libpostal.h"

SUITE(libpostal_parser_tests);

typedef struct labeled_component {
    char *label;
    char *component;
} labeled_component_t;

static greatest_test_res test_parse_result_equals(char *input, address_parser_options_t options, size_t output_len, ...) {
    address_parser_response_t *response = parse_address(input, options);

    va_list args;

    size_t i;
    if (output_len != response->num_components) {
        va_start(args, output_len);
        printf("Expected\n\n");
        for (i = 0; i < output_len; i++) {
            labeled_component_t lc = va_arg(args, labeled_component_t);
            printf("%s: %s\n", lc.label, lc.component);
        }
        printf("\n\n");
        printf("Got\n\n");
        for (i = 0; i < response->num_components; i++) {
            printf("%s: %s\n", response->labels[i], response->components[i]);
        }
        va_end(args);
        address_parser_response_destroy(response);
        FAIL();
    }

    va_start(args, output_len);

    for (i = 0; i < response->num_components; i++) {
        labeled_component_t lc = va_arg(args, labeled_component_t);

        ASSERT_STR_EQ(lc.label, response->labels[i]);
        ASSERT_STR_EQ(lc.component, response->components[i]);
    }

    va_end(args);

    address_parser_response_destroy(response);

    PASS();
}



TEST test_us_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "Barboncino 781 Franklin Ave Crown Heights Brooklyn NYC NY 11216 USA",
        options,
        9,  
        (labeled_component_t){"house", "barboncino"},
        (labeled_component_t){"house_number", "781"},
        (labeled_component_t){"road", "franklin ave"},
        (labeled_component_t){"suburb", "crown heights"},
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"city", "nyc"},
        (labeled_component_t){"state", "ny"},
        (labeled_component_t){"postcode", "11216"},
        (labeled_component_t){"country", "usa"}
    ));
    PASS();
}


TEST test_uk_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();


    CHECK_CALL(test_parse_result_equals(
        "The Book Club 100-106 Leonard St, Shoreditch, London, Greater London, England, EC2A 4RH, United Kingdom",
        options,
        9,
        (labeled_component_t){"house", "the book club"},
        (labeled_component_t){"house_number", "100-106"},
        (labeled_component_t){"road", "leonard st"},
        (labeled_component_t){"suburb", "shoreditch"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"state_district", "greater london"},
        (labeled_component_t){"state", "england"},
        (labeled_component_t){"postcode", "ec2a 4rh"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "The Book Club 100-106 Leonard St Shoreditch London EC2A 4RH United Kingdom",
        options,
        7,
        (labeled_component_t){"house", "the book club"},
        (labeled_component_t){"house_number", "100-106"},
        (labeled_component_t){"road", "leonard st"},
        (labeled_component_t){"suburb", "shoreditch"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"postcode", "ec2a 4rh"},
        (labeled_component_t){"country", "united kingdom"}
    ));


    PASS();
}


TEST test_es_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    // Use Spanish toponym
    CHECK_CALL(test_parse_result_equals(
        "Museo del Prado C. de Ruiz de Alarcón, 23 28014 Madrid, España",
        options,
        6,
        (labeled_component_t){"house", "museo del prado"},
        (labeled_component_t){"road", "c. de ruiz de alarcón"},
        (labeled_component_t){"house_number", "23"},
        (labeled_component_t){"postcode", "28014"},
        (labeled_component_t){"city", "madrid"},
        (labeled_component_t){"country", "españa"}
    ));

    // Use English toponym
    CHECK_CALL(test_parse_result_equals(
        "Museo del Prado C. de Ruiz de Alarcón, 23 28014 Madrid, Spain",
        options,
        6,
        (labeled_component_t){"house", "museo del prado"},
        (labeled_component_t){"road", "c. de ruiz de alarcón"},
        (labeled_component_t){"house_number", "23"},
        (labeled_component_t){"postcode", "28014"},
        (labeled_component_t){"city", "madrid"},
        (labeled_component_t){"country", "spain"}
    ));

    PASS();
}

TEST test_za_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Contains HTML entity which should be normalized
        // Contains 4-digit postcode, which can be confusable with a house number
        "Double Shot Tea &amp; Coffee 15 Melle St. Braamfontein Johannesburg, 2000, South Africa",
        options,
        7,
        (labeled_component_t){"house", "double shot tea & coffee"},
        (labeled_component_t){"house_number", "15"},
        (labeled_component_t){"road", "melle st."},
        (labeled_component_t){"suburb", "braamfontein"},
        (labeled_component_t){"city", "johannesburg"},
        (labeled_component_t){"postcode", "2000"},
        (labeled_component_t){"country", "south africa"}
    ));
    PASS();

}

TEST test_de_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        /* Contains Latin-ASCII normalizations
           Contains German concatenated street suffix

           N.B. We may want to move ä => ae out of the Latin-ASCII transliterator
           which will change the output of this test to e.g. eschenbräu bräurei
        */
        "Eschenbräu Bräurei Triftstraße 67 13353 Berlin Deutschland",
        options,
        6,
        (labeled_component_t){"house", "eschenbraeu braeurei"},
        (labeled_component_t){"road", "triftstrasse"},
        (labeled_component_t){"house_number", "67"},
        (labeled_component_t){"postcode", "13353"},
        (labeled_component_t){"city", "berlin"},
        (labeled_component_t){"country", "deutschland"}
    ));
    PASS();
}

TEST test_hu_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Hungarian, 4-digit postal code
        "Szimpla Kert Kazinczy utca 14 Budapest 1075, Magyarország",
        options,
        6,
        (labeled_component_t){"house", "szimpla kert"},
        (labeled_component_t){"road", "kazinczy utca"},
        (labeled_component_t){"house_number", "14"},
        (labeled_component_t){"city", "budapest"},
        (labeled_component_t){"postcode", "1075"},
        (labeled_component_t){"country", "magyarország"}
    ));
    PASS();
}


TEST test_ru_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Contains Cyrillic with abbreviations
        // Contains 6 digit postcode
        // Contains script change, English toponyms
        "Государственный Эрмитаж Дворцовая наб., 34 191186, St. Petersburg, Russia",
        options,
        6,
        (labeled_component_t){"house", "государственный эрмитаж"},
        (labeled_component_t){"road", "дворцовая наб."},
        (labeled_component_t){"house_number", "34"},
        (labeled_component_t){"postcode", "191186"},
        (labeled_component_t){"city", "st. petersburg"},
        (labeled_component_t){"country", "russia"}
    ));
    PASS();
}

SUITE(libpostal_parser_tests) {
    if (!libpostal_setup() || !libpostal_setup_parser()) {
        printf("Could not setup libpostal\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_us_parses);
    RUN_TEST(test_uk_parses);
    RUN_TEST(test_es_parses);
    RUN_TEST(test_za_parses);
    RUN_TEST(test_de_parses);
    RUN_TEST(test_hu_parses);
    RUN_TEST(test_ru_parses);

    libpostal_teardown();
    libpostal_teardown_parser();
}

