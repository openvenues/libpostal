#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/libpostal.h"
#include "../src/address_parser.h"

SUITE(libpostal_parser_tests);

typedef struct labeled_component {
    char *label;
    char *component;
} labeled_component_t;

static greatest_test_res test_parse_result_equals(char *input, address_parser_options_t options, size_t output_len, ...) {
    address_parser_response_t *response = parse_address(input, options);

    va_list args;

    size_t i;


    bool valid = output_len == response->num_components;

    if (valid) {
        va_start(args, output_len);

        for (i = 0; i < response->num_components; i++) {
            labeled_component_t lc = va_arg(args, labeled_component_t);

            if (!string_equals(lc.label, response->labels[i])) {
                valid = false;
                break;
            }
            if (!string_equals(lc.component, response->components[i])) {
                valid = false;
                break;
            }
        }

        va_end(args);
    }

    if (!valid) {
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

    address_parser_response_destroy(response);

    PASS();
}



TEST test_us_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Rare venue name without any common venue tokens following it
        // Neighborhood name
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

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/464
        "103 BEAL PKWY SE, FT WALTON BEACH, FL",
        options,
        4,
        (labeled_component_t){"house_number", "103"},
        (labeled_component_t){"road", "beal pkwy se"},
        (labeled_component_t){"city", "ft walton beach"},
        (labeled_component_t){"state", "fl"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/463
        "Canal Rd, Deltona FL",
        options,
        3,
        (labeled_component_t){"road", "canal rd"},
        (labeled_component_t){"city", "deltona"},
        (labeled_component_t){"state", "fl"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/125
        "123 Main St # 456 Oakland CA 94789",
        options,
        6,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st"},
        (labeled_component_t){"unit", "# 456"},
        (labeled_component_t){"city", "oakland"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "94789"}

    ));

    CHECK_CALL(test_parse_result_equals(
        "123 Main St Apt 456 Oakland CA 94789",
        options,
        6,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st"},
        (labeled_component_t){"unit", "apt 456"},
        (labeled_component_t){"city", "oakland"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "94789"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "123 Main St Apt #456 Oakland CA 94789",
        options,
        6,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st"},
        (labeled_component_t){"unit", "apt #456"},
        (labeled_component_t){"city", "oakland"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "94789"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "123 Main St Apt No. 456 Oakland CA 94789",
        options,
        6,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st"},
        (labeled_component_t){"unit", "apt no. 456"},
        (labeled_component_t){"city", "oakland"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "94789"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "whole foods nyc",
        options,
        2,
        (labeled_component_t){"house", "whole foods"},
        (labeled_component_t){"city", "nyc"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/427
        "921 83 street, nyc",
        options,
        3,
        (labeled_component_t){"house_number", "921"},
        (labeled_component_t){"road", "83 street"},
        (labeled_component_t){"city", "nyc"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/424
        "30 w 26 st",
        options,
        2,
        (labeled_component_t){"house_number", "30"},
        (labeled_component_t){"road", "w 26 st"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "30 West 26th Street Sixth Floor",
        options,
        3,
        (labeled_component_t){"house_number", "30"},
        (labeled_component_t){"road", "west 26th street"},
        (labeled_component_t){"level", "sixth floor"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "30 W 26th St 6th Fl",
        options,
        3,
        (labeled_component_t){"house_number", "30"},
        (labeled_component_t){"road", "w 26th st"},
        (labeled_component_t){"level", "6th fl"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/440
        "301 Commons Park S, Stamford, CT 06902",
        options,
        5,
        (labeled_component_t){"house_number", "301"},
        (labeled_component_t){"road", "commons park s"},
        (labeled_component_t){"city", "stamford"},
        (labeled_component_t){"state", "ct"},
        (labeled_component_t){"postcode", "06902"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        // House number range
        "912-914 8TH ST, CLARKSTON, WA 99403",
        options,
        5,
        (labeled_component_t){"house_number", "912-914"},
        (labeled_component_t){"road", "8th st"},
        (labeled_component_t){"city", "clarkston"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "99403"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        "2120 E Hill Street #104 Signal Hill CA 90755",
        options,
        6,
        (labeled_component_t){"house_number", "2120"},
        (labeled_component_t){"road", "e hill street"},
        (labeled_component_t){"unit", "#104"},
        (labeled_component_t){"city", "signal hill"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "90755"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        // 5-digit house number
        "12200 Montecito Road #H206 Seal Beach 90740",
        options,
        5,
        (labeled_component_t){"house_number", "12200"},
        (labeled_component_t){"road", "montecito road"},
        (labeled_component_t){"unit", "#h206"},
        (labeled_component_t){"city", "seal beach"},
        (labeled_component_t){"postcode", "90740"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        // space between mc and carroll
        "1036-1038 MC CARROLL ST CLARKSTON WA 99403",
        options,
        5,
        (labeled_component_t){"house_number", "1036-1038"},
        (labeled_component_t){"road", "mc carroll st"},
        (labeled_component_t){"city", "clarkston"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "99403"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        // hyphenated house number
        "2455-B W BENCH RD OTHELLO WA 99344",
        options,
        5,
        (labeled_component_t){"house_number", "2455-b"},
        (labeled_component_t){"road", "w bench rd"},
        (labeled_component_t){"city", "othello"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "99344"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/151
        // city name is part of street
        "473 Boston Rd, Wilbraham, MA",
        options,
        4,
        (labeled_component_t){"house_number", "473"},
        (labeled_component_t){"road", "boston rd"},
        (labeled_component_t){"city", "wilbraham"},
        (labeled_component_t){"state", "ma"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/45
        // House number is a valid postcode but not in context
        // Postcode is a ZIP+4 so have to rely on masked digits
        "25050 ALESSANDRO BLVD, STE B, MORENO VALLEY, CA, 92553-4313",
        options,
        6,
        (labeled_component_t){"house_number", "25050"},
        (labeled_component_t){"road", "alessandro blvd"},
        (labeled_component_t){"unit", "ste b"},
        (labeled_component_t){"city", "moreno valley"},
        (labeled_component_t){"state", "ca"},
        (labeled_component_t){"postcode", "92553-4313"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/21
        // PO box example
        "PO Box 1, Seattle, WA 98103",
        options,
        4,
        (labeled_component_t){"po_box", "po box 1"},
        (labeled_component_t){"city", "seattle"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "98103"}
    ));


    CHECK_CALL(test_parse_result_equals(
        "4411 Stone Way North Seattle, King County, WA 98103",
        options,
        6,
        (labeled_component_t){"house_number", "4411"},
        (labeled_component_t){"road", "stone way north"},
        (labeled_component_t){"city", "seattle"},
        (labeled_component_t){"state_district", "king county"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "98103"}
    ));


    // Tests of simple place names
    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/114
        "Columbus, OH",
        options,
        2,
        (labeled_component_t){"city", "columbus"},
        (labeled_component_t){"state", "oh"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/114
        "San Francisco CA",
        options,
        2,
        (labeled_component_t){"city", "san francisco"},
        (labeled_component_t){"state", "ca"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Common alternative name for San Francicso
        "SF CA",
        options,
        2,
        (labeled_component_t){"city", "sf"},
        (labeled_component_t){"state", "ca"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Carmel-by-the-Sea hyphenated
        "Carmel-by-the-Sea, CA",
        options,
        2,
        (labeled_component_t){"city", "carmel-by-the-sea"},
        (labeled_component_t){"state", "ca"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Carmel-by-the-Sea de-hyphenated
        "Carmel by the Sea, CA",
        options,
        2,
        (labeled_component_t){"city", "carmel by the sea"},
        (labeled_component_t){"state", "ca"}
    ));

    // Disambiguation tests

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/53
        // Manhattan as city_district
        "Manhattan, NY",
        options,
        2,
        (labeled_component_t){"city_district", "manhattan"},
        (labeled_component_t){"state", "ny"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Manhattan, Kansas - city
        "Manhattan, KS",
        options,
        2,
        (labeled_component_t){"city", "manhattan"},
        (labeled_component_t){"state", "ks"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Brooklyn, NY - city_district
        "Brooklyn, NY",
        options,
        2,
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"state", "ny"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Brooklyn, Connecticut - city
        "Brooklyn CT",
        options,
        2,
        (labeled_component_t){"city", "brooklyn"},
        (labeled_component_t){"state", "ct"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Here Brooklyn CT means "Brooklyn Court", a small street in Oregon
        "18312 SE Brooklyn CT Gresham OR",
        options,
        4,
        (labeled_component_t){"house_number", "18312"},
        (labeled_component_t){"road", "se brooklyn ct"},
        (labeled_component_t){"city", "gresham"},
        (labeled_component_t){"state", "or"}
    ));

    PASS();
}

TEST test_ca_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/55
        "332 Menzies Street, Victoria, BC V8V 2G9",
        options,
        5,
        (labeled_component_t){"house_number", "332"},
        (labeled_component_t){"road", "menzies street"},
        (labeled_component_t){"city", "victoria"},
        (labeled_component_t){"state", "bc"},
        (labeled_component_t){"postcode", "v8v 2g9"}
    ));
}

TEST test_jm_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/113
        // Kingston postcode, rare case where single-digit number is a postcode
        // Uses W.I for "West Indies"
        "237 Old Hope Road, Kingston 6, Jamaica W.I",
        options,
        6,
        (labeled_component_t){"house_number", "237"},
        (labeled_component_t){"road", "old hope road"},
        (labeled_component_t){"city", "kingston"},
        (labeled_component_t){"postcode", "6"},
        (labeled_component_t){"country", "jamaica"},
        (labeled_component_t){"world_region", "w.i"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/113
        // Fractional house number
        "16 1/2 Windward Road, Kingston 2, Jamaica",
        options,
        5,
        (labeled_component_t){"house_number", "16 1/2"},
        (labeled_component_t){"road", "windward road"},
        (labeled_component_t){"city", "kingston"},
        (labeled_component_t){"postcode", "2"},
        (labeled_component_t){"country", "jamaica"}

    ));

}


TEST test_gb_parses(void) {
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

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openownership/data-standard/issues/18
        "Aston House, Cornwall Avenue, London, N3 1LF",
        options,
        4,
        (labeled_component_t){"house", "aston house"},
        (labeled_component_t){"road", "cornwall avenue"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"postcode", "n3 1lf"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/39
        "318 Upper Street, N1 2XQ London",
        options,
        4,
        (labeled_component_t){"house_number", "318"},
        (labeled_component_t){"road", "upper street"},
        (labeled_component_t){"postcode", "n1 2xq"},
        (labeled_component_t){"city", "london"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/39
        "21, Kingswood Road SW2 4JE, London",
        options,
        4,
        (labeled_component_t){"house_number", "21"},
        (labeled_component_t){"road", "kingswood road"},
        (labeled_component_t){"postcode", "sw2 4je"},
        (labeled_component_t){"city", "london"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From Moz tests
        "1 Riverside Dr Liverpool, Merseyside L3 4EN",
        options,
        5,
        (labeled_component_t){"house_number", "1"},
        (labeled_component_t){"road", "riverside dr"},
        (labeled_component_t){"city", "liverpool"},
        (labeled_component_t){"state_district", "merseyside"},
        (labeled_component_t){"postcode", "l3 4en"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Knutsford, Cheshire East WA16 9EX, UK",
        options,
        4,
        (labeled_component_t){"city", "knutsford"},
        (labeled_component_t){"state_district", "cheshire east"},
        (labeled_component_t){"postcode", "wa16 9ex"},
        (labeled_component_t){"country", "uk"}
    ));

    PASS();
}

TEST test_nz_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "wellington new zealand",
        options,
        2,
        (labeled_component_t){"city", "wellington"},
        (labeled_component_t){"country", "new zealand"}
    ));

    PASS();
}

TEST test_fr_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();
    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/426
        "Chambéry",
        options,
        1,
        (labeled_component_t){"city", "chambéry"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/pelias/pelias/issues/426
        "Chambery",
        options,
        1,
        (labeled_component_t){"city", "chambery"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/114
        "Paris, France",
        options,
        2,
        (labeled_component_t){"city", "paris"},
        (labeled_component_t){"country", "france"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Variant of above
        "Paris",
        options,
        1,
        (labeled_component_t){"city", "paris"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Variant of above
        "Paris, FR",
        options,
        2,
        (labeled_component_t){"city", "paris"},
        (labeled_component_t){"country", "fr"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Arrondissement Roman numerals
        "IXe arrondissement Paris",
        options,
        2,
        (labeled_component_t){"city_district", "ixe arrondissement"},
        (labeled_component_t){"city", "paris"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Arrondissement Arabic numerals
        "9e arrondissement Paris",
        options,
        2,
        (labeled_component_t){"city_district", "9e arrondissement"},
        (labeled_component_t){"city", "paris"}
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
        (labeled_component_t){"road", "c. de ruiz de alarcón"},
        (labeled_component_t){"house_number", "23"},
        (labeled_component_t){"postcode", "28014"},
        (labeled_component_t){"city", "madrid"},
        (labeled_component_t){"country", "españa"}
    ));

    // Use English toponym
    CHECK_CALL(test_parse_result_equals(
        "Museo del Prado C. de Ruiz de Alarcón, 23 28014 Madrid, Spain",
        options,
        6,
        (labeled_component_t){"house", "museo del prado"},
        (labeled_component_t){"road", "c. de ruiz de alarcón"},
        (labeled_component_t){"house_number", "23"},
        (labeled_component_t){"postcode", "28014"},
        (labeled_component_t){"city", "madrid"},
        (labeled_component_t){"country", "spain"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Spanish-style floor number
        "Paseo de la Castellana, 185 - 5º, 28046 Madrid Madrid",
        options,
        6,
        (labeled_component_t){"road", "paseo de la castellana"},
        (labeled_component_t){"house_number", "185"},
        (labeled_component_t){"level", "5º"},
        (labeled_component_t){"postcode", "28046"},
        (labeled_component_t){"city", "madrid"},
        (labeled_component_t){"state", "madrid"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Spanish-style floor number + side (unit)
        "Av. de las Delicias, 14, 1º Dcha, 28045 Madrid",
        options,
        6,
        (labeled_component_t){"road", "av. de las delicias"},
        (labeled_component_t){"house_number", "14"},
        (labeled_component_t){"level", "1º"},
        (labeled_component_t){"unit", "dcha"},
        (labeled_component_t){"postcode", "28045"},
        (labeled_component_t){"city", "madrid"}
    ));
    PASS();
}

TEST test_co_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "Cra 18#63-64 B Chapinero Bogotá DC Colombia",
        options,
        5,
        (labeled_component_t){"road", "cra 18"},
        (labeled_component_t){"house_number", "#63-64 b"},
        (labeled_component_t){"city_district", "chapinero"},
        (labeled_component_t){"city", "bogotá dc"},
        (labeled_component_t){"country", "colombia"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Bogotá Colombia",
        options,
        2,
        (labeled_component_t){"city", "bogotá"},
        (labeled_component_t){"country", "colombia"}
    ));

    // Test with country code (could also be Colorado, company, etc.)
    CHECK_CALL(test_parse_result_equals(
        "Bogotá CO",
        options,
        2,
        (labeled_component_t){"city", "bogotá"},
        (labeled_component_t){"country", "co"}
    ));

    // Same tests without accent
    CHECK_CALL(test_parse_result_equals(
        "Cra 18#63-64 B Chapinero Bogota DC Colombia",
        options,
        5,
        (labeled_component_t){"road", "cra 18"},
        (labeled_component_t){"house_number", "#63-64 b"},
        (labeled_component_t){"city_district", "chapinero"},
        (labeled_component_t){"city", "bogota dc"},
        (labeled_component_t){"country", "colombia"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Bogota Colombia",
        options,
        2,
        (labeled_component_t){"city", "bogota"},
        (labeled_component_t){"country", "colombia"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Bogota CO",
        options,
        2,
        (labeled_component_t){"city", "bogota"},
        (labeled_component_t){"country", "co"}
    ));


    PASS();
}

TEST test_mx_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    // From: https://github.com/openvenues/libpostal/issues/126
    CHECK_CALL(test_parse_result_equals(
        "LÓPEZ MATEOS, 106, 21840, MEXICALI, baja-california, mx",
        options,
        6,
        (labeled_component_t){"road", "lópez mateos"},
        (labeled_component_t){"house_number", "106"},
        (labeled_component_t){"postcode", "21840"},
        (labeled_component_t){"city", "mexicali"},
        (labeled_component_t){"state", "baja-california"},
        (labeled_component_t){"country", "mx"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "LORENZO DE ZOVELA, 1126, 22715, PLAYAS DE ROSARITO, baja-california, mx",
        options,
        6,
        (labeled_component_t){"road", "lorenzo de zovela"},
        (labeled_component_t){"house_number", "1126"},
        (labeled_component_t){"postcode", "22715"},
        (labeled_component_t){"city", "playas de rosarito"},
        (labeled_component_t){"state", "baja-california"},
        (labeled_component_t){"country", "mx"}
    ));

    PASS();
}


TEST test_cn_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/71
        // Level, unit, road name containing a city (Hong Kong)
        "中国，山东省，青岛市 香港东路6号，5号楼，8号室 李小方 先生收",
        options,
        8,
        (labeled_component_t){"country", "中国"},
        (labeled_component_t){"state", "山东省"},
        (labeled_component_t){"city", "青岛市"},
        (labeled_component_t){"road", "香港东路"},
        (labeled_component_t){"house_number", "6号"},
        (labeled_component_t){"level", "5号楼"},
        (labeled_component_t){"unit", "8号室"},
        (labeled_component_t){"house", "李小方 先生收"}
    ));
    PASS();
}



TEST test_jp_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Example of a Kanji address
        "〒601-8446京都市南区西九条高畠町25-1京都醸造株式会社",
        options,
        6,
        (labeled_component_t){"postcode", "〒601-8446"},
        (labeled_component_t){"city", "京都市"},
        (labeled_component_t){"city_district", "南区"},
        (labeled_component_t){"suburb", "西九条高畠町"},
        (labeled_component_t){"house_number", "25-1"},
        (labeled_component_t){"house", "京都醸造株式会社"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Ban-go style house number, level and unit
        "日本国〒113-0001文京区4丁目3番2号3階323号室",
        options,
        7,
        (labeled_component_t){"country", "日本国"},
        (labeled_component_t){"postcode", "〒113-0001"},
        (labeled_component_t){"city", "文京区"},
        (labeled_component_t){"suburb", "4丁目"},
        (labeled_component_t){"house_number", "3番2号"},
        (labeled_component_t){"level", "3階"},
        (labeled_component_t){"unit", "323号室"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/123
        // University (slightly ambiguous i.e. the 2nd "Osaka" can be part of a campus name)
        // English toponyms
        "osaka university, osaka, japan, 565-0871",
        options,
        4,
        (labeled_component_t){"house", "osaka university"},
        (labeled_component_t){"city", "osaka"},
        (labeled_component_t){"country", "japan"},
        (labeled_component_t){"postcode", "565-0871"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/62
        // Romaji
        // Has road name (I think?)
        "135, Higashifunahashi 2 Chome, Hirakata-shi Osaka-fu",
        options,
        5,
        (labeled_component_t){"house_number", "135"},
        (labeled_component_t){"road", "higashifunahashi"},
        (labeled_component_t){"suburb", "2 chome"},
        (labeled_component_t){"city", "hirakata-shi"},
        (labeled_component_t){"state", "osaka-fu"}

    ));
    PASS();
}

TEST test_kr_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // English/Romanized Korean, ro + gil address, English unit
        "Suite 1005, 36, Teheran-ro 87-gil, Gangnam-gu Seoul 06164 Republic of Korea",
        options,
        7,
        (labeled_component_t){"unit", "suite 1005"},
        (labeled_component_t){"house_number", "36"},
        (labeled_component_t){"road", "teheran-ro 87-gil"},
        (labeled_component_t){"city_district", "gangnam-gu"},
        (labeled_component_t){"city", "seoul"},
        (labeled_component_t){"postcode", "06164"},
        (labeled_component_t){"country", "republic of korea"}
    ));
    PASS();
}

TEST test_my_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/121
        // Not adding the block format yet in case we change how it's parsed
        "IBS Centre Jalan Chan Sow Lin, 55200 Kuala Lumpur, Malaysia",
        options,
        5,
        (labeled_component_t){"house", "ibs centre"},
        (labeled_component_t){"road", "jalan chan sow lin"},
        (labeled_component_t){"postcode", "55200"},
        (labeled_component_t){"city", "kuala lumpur"},
        (labeled_component_t){"country", "malaysia"}
    ));
    PASS();
}

TEST test_za_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Contains HTML entity which should be normalized
        // Contains 4-digit postcode, which can be confusable with a house number
        "Double Shot Tea &amp; Coffee 15 Melle St. Braamfontein Johannesburg, 2001, South Africa",
        options,
        7,
        (labeled_component_t){"house", "double shot tea & coffee"},
        (labeled_component_t){"house_number", "15"},
        (labeled_component_t){"road", "melle st."},
        (labeled_component_t){"suburb", "braamfontein"},
        (labeled_component_t){"city", "johannesburg"},
        (labeled_component_t){"postcode", "2001"},
        (labeled_component_t){"country", "south africa"}
    ));
    PASS();

}

TEST test_de_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        /* Contains German concatenated street suffix

           N.B. We may want to move ä => ae out of the Latin-ASCII transliterator
           which will change the output of this test to e.g. eschenbräu bräurei
        */
        "Eschenbräu Bräurei Triftstraße 67 13353 Berlin Deutschland",
        options,
        6,
        (labeled_component_t){"house", "eschenbräu bräurei"},
        (labeled_component_t){"road", "triftstraße"},
        (labeled_component_t){"house_number", "67"},
        (labeled_component_t){"postcode", "13353"},
        (labeled_component_t){"city", "berlin"},
        (labeled_component_t){"country", "deutschland"}
    ));

    // Test transliterated versions
    CHECK_CALL(test_parse_result_equals(
        "Eschenbrau Braurei Triftstrasse 67 13353 Berlin Deutschland",
        options,
        6,
        (labeled_component_t){"house", "eschenbrau braurei"},
        (labeled_component_t){"road", "triftstrasse"},
        (labeled_component_t){"house_number", "67"},
        (labeled_component_t){"postcode", "13353"},
        (labeled_component_t){"city", "berlin"},
        (labeled_component_t){"country", "deutschland"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Eschenbraeu Braeurei Triftstrasse 67 13353 Berlin DE",
        options,
        6,
        (labeled_component_t){"house", "eschenbraeu braeurei"},
        (labeled_component_t){"road", "triftstrasse"},
        (labeled_component_t){"house_number", "67"},
        (labeled_component_t){"postcode", "13353"},
        (labeled_component_t){"city", "berlin"},
        (labeled_component_t){"country", "de"}
    ));

    PASS();
}


TEST test_at_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "Eduard Sueß Gasse 9",
        options,
        2,
        (labeled_component_t){"road", "eduard sueß gasse"},
        (labeled_component_t){"house_number", "9"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Eduard-Sueß Gasse 9",
        options,
        2,
        (labeled_component_t){"road", "eduard-sueß gasse"},
        (labeled_component_t){"house_number", "9"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Eduard-Sueß-Gasse 9",
        options,
        2,
        (labeled_component_t){"road", "eduard-sueß-gasse"},
        (labeled_component_t){"house_number", "9"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Eduard Sueß-Gasse 9",
        options,
        2,
        (labeled_component_t){"road", "eduard sueß-gasse"},
        (labeled_component_t){"house_number", "9"}
    ));

    // From https://github.com/openvenues/libpostal/issues/128
    CHECK_CALL(test_parse_result_equals(
        "Wien, Österreich",
        options,
        2,
        (labeled_component_t){"city", "wien"},
        (labeled_component_t){"country", "österreich"}
    ));

    // Transliterations
    CHECK_CALL(test_parse_result_equals(
        "Wien, Osterreich",
        options,
        2,
        (labeled_component_t){"city", "wien"},
        (labeled_component_t){"country", "osterreich"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Wien, Oesterreich",
        options,
        2,
        (labeled_component_t){"city", "wien"},
        (labeled_component_t){"country", "oesterreich"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // English names
        "Vienna, Austria",
        options,
        2,
        (labeled_component_t){"city", "vienna"},
        (labeled_component_t){"country", "austria"}
    ));

    PASS();
}


TEST test_nl_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();
    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/162
        "Nieuwe Binnenweg 17-19, Oude Westen, Rotterdam NL",
        options,
        5,
        (labeled_component_t){"road", "nieuwe binnenweg"},
        (labeled_component_t){"house_number", "17-19"},
        (labeled_component_t){"suburb", "oude westen"},
        (labeled_component_t){"city", "rotterdam"},  
        (labeled_component_t){"country", "nl"}  
    ));

    CHECK_CALL(test_parse_result_equals(
        "Nieuwe Binnenweg 17-19, Oude Westen, Rotterdam",
        options,
        4,
        (labeled_component_t){"road", "nieuwe binnenweg"},
        (labeled_component_t){"house_number", "17-19"},
        (labeled_component_t){"suburb", "oude westen"},
        (labeled_component_t){"city", "rotterdam"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Oude Westen, Rotterdam",
        options,
        2,
        (labeled_component_t){"suburb", "oude westen"},
        (labeled_component_t){"city", "rotterdam"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/75
        "Olympia 1 A begane gro",
        options,
        3,
        (labeled_component_t){"road", "olympia"},
        (labeled_component_t){"house_number", "1 a"},
        (labeled_component_t){"level", "begane gro"}
    ));

    PASS();
}

TEST test_da_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "Valdemarsgade 42 4 t.v. København, 1665 Danmark",
        options,
        6,
        (labeled_component_t){"road", "valdemarsgade"},
        (labeled_component_t){"house_number", "42"},
        (labeled_component_t){"unit", "4 t.v."},
        (labeled_component_t){"city", "københavn"},
        (labeled_component_t){"postcode", "1665"},
        (labeled_component_t){"country", "danmark"}
    ));

    PASS();
}

TEST test_fi_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "1 Hämeenkatu, Tampere, Finland",
        options,
        4,
        (labeled_component_t){"house_number", "1"},
        (labeled_component_t){"road", "hämeenkatu"},
        (labeled_component_t){"city", "tampere"},
        (labeled_component_t){"country", "finland"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/111
        "Pitkämäentie",
        options,
        1,
        (labeled_component_t){"road", "pitkämäentie"}
    ));

    PASS();
}

TEST test_no_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/39#issuecomment-221027220
        "Sars gate 2 A, 562 OSLO",
        options,
        4,
        (labeled_component_t){"road", "sars gate"},
        (labeled_component_t){"house_number", "2 a"},
        (labeled_component_t){"postcode", "562"},
        (labeled_component_t){"city", "oslo"}
    ));
    PASS();
}

TEST test_se_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Uses the "en trappa upp" (one floor up) form in Swedish addresses
        "Storgatan 1 en trappa upp 112 01 Stockholm Sweden",
        options,
        6,
        (labeled_component_t){"road", "storgatan"},
        (labeled_component_t){"house_number", "1"},
        (labeled_component_t){"level", "en trappa upp"},
        (labeled_component_t){"postcode", "112 01"},
        (labeled_component_t){"city", "stockholm"},
        (labeled_component_t){"country", "sweden"}
    ));
    PASS();
}

TEST test_hu_parses(void) {
    address_parser_options_t options = get_libpostal_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Hungarian, 4-digit postal code
        "1075 Budapest kazinczy utca, 14",
        options,
        4,
        (labeled_component_t){"postcode", "1075"},
        (labeled_component_t){"city", "budapest"},
        (labeled_component_t){"road", "kazinczy utca"},
        (labeled_component_t){"house_number", "14"}
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
        (labeled_component_t){"house", "государственный эрмитаж"},
        (labeled_component_t){"road", "дворцовая наб."},
        (labeled_component_t){"house_number", "34"},
        (labeled_component_t){"postcode", "191186"},
        (labeled_component_t){"city", "st. petersburg"},
        (labeled_component_t){"country", "russia"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/138
        "Петрозаводск Карелия Российская Федерация",
        options,
        3,
        (labeled_component_t){"city", "петрозаводск"},
        (labeled_component_t){"state", "карелия"},
        (labeled_component_t){"country", "российская федерация"}
    ));


    CHECK_CALL(test_parse_result_equals(
        // From https://github.com/openvenues/libpostal/issues/138
        "Автолюбителейроезд 24 Петрозаводск Карелия Российская Федерация 185013",
        options,
        6,
        (labeled_component_t){"road", "автолюбителейроезд"},
        (labeled_component_t){"house_number", "24"},
        (labeled_component_t){"city", "петрозаводск"},
        (labeled_component_t){"state", "карелия"},
        (labeled_component_t){"country", "российская федерация"},
        (labeled_component_t){"postcode", "185013"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Old Soviet format, from https://github.com/openvenues/libpostal/issues/125#issuecomment-269319652
        // Uses "г."" prefix for the city
        // Uses "д." for house number
        // Has apartment number with "кв."
        "197198, г. Санкт-Петербург, ул. Съезжинская д. 10 кв. 40",
        options,
        5,
        (labeled_component_t){"postcode", "197198"},
        (labeled_component_t){"city", "г. санкт-петербург"},
        (labeled_component_t){"road", "ул. съезжинская"},
        (labeled_component_t){"house_number", "д. 10"},
        (labeled_component_t){"unit", "кв. 40"}
    ));

    /*
    CHECK_CALL(test_parse_result_equals(
        // Uses genitive place names, see https://github.com/openvenues/libpostal/issues/125#issuecomment-269438636
        "188541, г. Сосновый Бор Ленинградской области, пр. Героев 40, кв. 400",
        options,
        6,
        (labeled_component_t){"postcode", "188541"},
        (labeled_component_t){"city", "г. сосновый бор"},
        (labeled_component_t){"state", "ленинградской области"},
        (labeled_component_t){"road", "пр. героев"},
        (labeled_component_t){"house_number", "40"},
        (labeled_component_t){"unit", "кв. 400"}
    ));
    */

    PASS();
}

SUITE(libpostal_parser_tests) {
    if (!libpostal_setup() || !libpostal_setup_parser()) {
        printf("Could not setup libpostal\n");
        exit(EXIT_FAILURE);
    }

    RUN_TEST(test_us_parses);
    RUN_TEST(test_jm_parses);
    RUN_TEST(test_gb_parses);
    RUN_TEST(test_nz_parses);
    RUN_TEST(test_fr_parses);
    RUN_TEST(test_es_parses);
    RUN_TEST(test_co_parses);
    RUN_TEST(test_mx_parses);
    RUN_TEST(test_cn_parses);
    RUN_TEST(test_jp_parses);
    RUN_TEST(test_kr_parses);
    RUN_TEST(test_my_parses);
    RUN_TEST(test_za_parses);
    RUN_TEST(test_de_parses);
    RUN_TEST(test_at_parses);
    RUN_TEST(test_nl_parses);
    RUN_TEST(test_da_parses);
    RUN_TEST(test_fi_parses);
    RUN_TEST(test_no_parses);
    RUN_TEST(test_se_parses);
    RUN_TEST(test_hu_parses);
    RUN_TEST(test_ru_parses);

    libpostal_teardown();
    libpostal_teardown_parser();
}
