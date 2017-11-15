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

static greatest_test_res test_parse_result_equals(char *input, libpostal_address_parser_options_t options, size_t output_len, ...) {
    libpostal_address_parser_response_t *response = libpostal_parse_address(input, options);

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
        libpostal_address_parser_response_destroy(response);
        FAIL();
    }

    libpostal_address_parser_response_destroy(response);

    PASS();
}



TEST test_us_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        "Black Alliance for Just Immigration 660 Nostrand Ave, Brooklyn, N.Y., 11216",
        options,
        6,
        (labeled_component_t){"house", "black alliance for just immigration"},
        (labeled_component_t){"house_number", "660"},
        (labeled_component_t){"road", "nostrand ave"},
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"state", "n.y."},
        (labeled_component_t){"postcode", "11216"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Planned Parenthood, 44 Court St, 6th Floor, Brooklyn 11201",
        options,
        6,
        (labeled_component_t){"house", "planned parenthood"},
        (labeled_component_t){"house_number", "44"},
        (labeled_component_t){"road", "court st"},
        (labeled_component_t){"level", "6th floor"},
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"postcode", "11201"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Congresswoman Yvette Clarke 222 Lenox Road, Ste 1 Brooklyn New York 11226",
        options,
        7,
        (labeled_component_t){"house", "congresswoman yvette clarke"},
        (labeled_component_t){"house_number", "222"},
        (labeled_component_t){"road", "lenox road"},
        (labeled_component_t){"unit", "ste 1"},
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"state", "new york"},
        (labeled_component_t){"postcode", "11226"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "ACLU DC P.O. Box 11637 Washington, DC 20008 United States",
        options,
        6,
        (labeled_component_t){"house", "aclu dc"},
        (labeled_component_t){"po_box", "p.o. box 11637"},
        (labeled_component_t){"city", "washington"},
        (labeled_component_t){"state", "dc"},
        (labeled_component_t){"postcode", "20008"},
        (labeled_component_t){"country", "united states"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Make the Road New York 92-10 Roosevelt Avenue Jackson Heights Queens 11372",
        options,
        6,
        (labeled_component_t){"house", "make the road new york"},
        (labeled_component_t){"house_number", "92-10"},
        (labeled_component_t){"road", "roosevelt avenue"},
        (labeled_component_t){"suburb", "jackson heights"},
        (labeled_component_t){"city_district", "queens"},
        (labeled_component_t){"postcode", "11372"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Do the Right Thing Way, Bed-Stuy, BK",
        options,
        3,
        (labeled_component_t){"road", "do the right thing way"},
        (labeled_component_t){"suburb", "bed-stuy"},
        (labeled_component_t){"city_district", "bk"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "book stores near me",
        options,
        2,
        (labeled_component_t){"category", "book stores"},
        (labeled_component_t){"near", "near me"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "theatres in Fort Greene Brooklyn",
        options,
        4,
        (labeled_component_t){"category", "theatres"},
        (labeled_component_t){"near", "in"},
        (labeled_component_t){"suburb", "fort greene"},
        (labeled_component_t){"city_district", "brooklyn"}
    ));

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

    CHECK_CALL(test_parse_result_equals(
        // newline
        "452 Maxwell Ave, Apt 3A\nRochester, NY 14619",
        options,
        6,
        (labeled_component_t){"house_number", "452"},
        (labeled_component_t){"road", "maxwell ave"},
        (labeled_component_t){"unit", "apt 3a"},
        (labeled_component_t){"city", "rochester"},
        (labeled_component_t){"state", "ny"},
        (labeled_component_t){"postcode", "14619"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "1600 Pennsylvania Ave NW, Washington DC 20500",
        options,
        5,
        (labeled_component_t){"house_number", "1600"},
        (labeled_component_t){"road", "pennsylvania ave nw"},
        (labeled_component_t){"city", "washington"},
        (labeled_component_t){"state", "dc"},
        (labeled_component_t){"postcode", "20500"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "1600 Pennsylvania Ave NW, Washington D.C 20500",
        options,
        5,
        (labeled_component_t){"house_number", "1600"},
        (labeled_component_t){"road", "pennsylvania ave nw"},
        (labeled_component_t){"city", "washington"},
        (labeled_component_t){"state", "d.c"},
        (labeled_component_t){"postcode", "20500"}
    ));


    CHECK_CALL(test_parse_result_equals(
        "1600 Pennsylvania Ave NW, Washington D.C. 20500",
        options,
        5,
        (labeled_component_t){"house_number", "1600"},
        (labeled_component_t){"road", "pennsylvania ave nw"},
        (labeled_component_t){"city", "washington"},
        (labeled_component_t){"state", "d.c."},
        (labeled_component_t){"postcode", "20500"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Queens address
        "99-40 63rd Rd, Queens, NY 11374",
        options,
        5,
        (labeled_component_t){"house_number", "99-40"},
        (labeled_component_t){"road", "63rd rd"},
        (labeled_component_t){"city_district", "queens"},
        (labeled_component_t){"state", "ny"},
        (labeled_component_t){"postcode", "11374"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Prefix directional
        "351 NW North St, Chehalis, WA 98532-1900",
        options,
        5,
        (labeled_component_t){"house_number", "351"},
        (labeled_component_t){"road", "nw north st"},
        (labeled_component_t){"city", "chehalis"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "98532-1900"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // full state name
        "2501 N Blackwelder Ave, Oklahoma City, Oklahoma 73106",
        options,
        5,
        (labeled_component_t){"house_number", "2501"},
        (labeled_component_t){"road", "n blackwelder ave"},
        (labeled_component_t){"city", "oklahoma city"},
        (labeled_component_t){"state", "oklahoma"},
        (labeled_component_t){"postcode", "73106"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // disambiguation: less common form of Indiana, usually a state
        "1011 South Dr, Indiana, Pennsylvania 15705",
        options,
        5,
        (labeled_component_t){"house_number", "1011"},
        (labeled_component_t){"road", "south dr"},
        (labeled_component_t){"city", "indiana"},
        (labeled_component_t){"state", "pennsylvania"},
        (labeled_component_t){"postcode", "15705"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Different form of N.Y.
        "444 South 5th St Apt. 3A Brooklyn, N.Y. 11211",
        options,
        6,
        (labeled_component_t){"house_number", "444"},
        (labeled_component_t){"road", "south 5th st"},
        (labeled_component_t){"unit", "apt. 3a"},
        (labeled_component_t){"city_district", "brooklyn"},
        (labeled_component_t){"state", "n.y."},
        (labeled_component_t){"postcode", "11211"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Atrium Mall, 640 Arthur Kill Rd, Staten Island, NY 10312",
        options,
        6,
        (labeled_component_t){"house", "atrium mall"},
        (labeled_component_t){"house_number", "640"},
        (labeled_component_t){"road", "arthur kill rd"},
        (labeled_component_t){"city_district", "staten island"},
        (labeled_component_t){"state", "ny"},
        (labeled_component_t){"postcode", "10312"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "5276 Old Mill Rd NE, Bainbridge Island, WA 98110",
        options,
        5,
        (labeled_component_t){"house_number", "5276"},
        (labeled_component_t){"road", "old mill rd ne"},
        (labeled_component_t){"city", "bainbridge island"},
        (labeled_component_t){"state", "wa"},
        (labeled_component_t){"postcode", "98110"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "1400 West Transport Road, Fayetteville, AR, 72704",
        options,
        5,
        (labeled_component_t){"house_number", "1400"},
        (labeled_component_t){"road", "west transport road"},
        (labeled_component_t){"city", "fayetteville"},
        (labeled_component_t){"state", "ar"},
        (labeled_component_t){"postcode", "72704"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "10 Amelia Village Circle, Fernandina Beach, FL, 32034",
        options,
        5,
        (labeled_component_t){"house_number", "10"},
        (labeled_component_t){"road", "amelia village circle"},
        (labeled_component_t){"city", "fernandina beach"},
        (labeled_component_t){"state", "fl"},
        (labeled_component_t){"postcode", "32034"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // highway address
        "5850 US Highway 431, STE 1, Albertville, AL, 35950-2049",
        options,
        6,
        (labeled_component_t){"house_number", "5850"},
        (labeled_component_t){"road", "us highway 431"},
        (labeled_component_t){"unit", "ste 1"},
        (labeled_component_t){"city", "albertville"},
        (labeled_component_t){"state", "al"},
        (labeled_component_t){"postcode", "35950-2049"}
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
        "Brooklyn, CT 06234",
        options,
        3,
        (labeled_component_t){"city", "brooklyn"},
        (labeled_component_t){"state", "ct"},
        (labeled_component_t){"postcode", "06234"}
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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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

    // Montreal / Montréal

    CHECK_CALL(test_parse_result_equals(
        "123 Main St SE\nMontreal QC H3Z 2Y7",
        options,
        5,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st se"},
        (labeled_component_t){"city", "montreal"},
        (labeled_component_t){"state", "qc"},
        (labeled_component_t){"postcode", "h3z 2y7"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "123 Main St SE Montréal QC H3Z 2Y7",
        options,
        5,
        (labeled_component_t){"house_number", "123"},
        (labeled_component_t){"road", "main st se"},
        (labeled_component_t){"city", "montréal"},
        (labeled_component_t){"state", "qc"},
        (labeled_component_t){"postcode", "h3z 2y7"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/pelias/pelias/issues/275
        "LaSalle Montréal QC",
        options,
        3,
        (labeled_component_t){"suburb", "lasalle"},
        (labeled_component_t){"city", "montréal"},
        (labeled_component_t){"state", "qc"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/pelias/pelias/issues/275
        "LaSalle Montreal QC",
        options,
        3,
        (labeled_component_t){"suburb", "lasalle"},
        (labeled_component_t){"city", "montreal"},
        (labeled_component_t){"state", "qc"}
    ));


    PASS();
}

TEST test_jm_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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

    CHECK_CALL(test_parse_result_equals(
        "16½ Windward Road Kingston 2 Jamaica, West Indies",
        options,
        6,
        (labeled_component_t){"house_number", "16½"},
        (labeled_component_t){"road", "windward road"},
        (labeled_component_t){"city", "kingston"},
        (labeled_component_t){"postcode", "2"},
        (labeled_component_t){"country", "jamaica"},
        (labeled_component_t){"world_region", "west indies"}
    ));

    PASS();
}


TEST test_gb_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();


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
        "Stocks Ln, Knutsford, Cheshire East WA16 9EX, UK",
        options,
        5,
        (labeled_component_t){"road", "stocks ln"},
        (labeled_component_t){"city", "knutsford"},
        (labeled_component_t){"state_district", "cheshire east"},
        (labeled_component_t){"postcode", "wa16 9ex"},
        (labeled_component_t){"country", "uk"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Royal Opera House, Bow St, Covent Garden, London, WC2E 9DD, United Kingdom",
        options,
        6,
        (labeled_component_t){"house", "royal opera house"},
        (labeled_component_t){"road", "bow st"},
        (labeled_component_t){"suburb", "covent garden"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"postcode", "wc2e 9dd"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "1A Egmont Road, Middlesbrough, TS4 2HT",
        options,
        4,
        (labeled_component_t){"house_number", "1a"},
        (labeled_component_t){"road", "egmont road"},
        (labeled_component_t){"city", "middlesbrough"},
        (labeled_component_t){"postcode", "ts4 2ht"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "0 Egmont Road, Middlesbrough, TS4 2HT",
        options,
        4,
        (labeled_component_t){"house_number", "0"},
        (labeled_component_t){"road", "egmont road"},
        (labeled_component_t){"city", "middlesbrough"},
        (labeled_component_t){"postcode", "ts4 2ht"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "-1 Priory Road, Newbury, RG14 7QS",
        options,
        4,
        (labeled_component_t){"house_number", "-1"},
        (labeled_component_t){"road", "priory road"},
        (labeled_component_t){"city", "newbury"},
        (labeled_component_t){"postcode", "rg14 7qs"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Idas Court, 4-6 Princes Road, Hull, HU5 2RD",
        options,
        5,
        (labeled_component_t){"house", "idas court"},
        (labeled_component_t){"house_number", "4-6"},
        (labeled_component_t){"road", "princes road"},
        (labeled_component_t){"city", "hull"},
        (labeled_component_t){"postcode", "hu5 2rd"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Flat 14, Ziggurat Building, 60-66 Saffron Hill, London, EC1N 8QX, United Kingdom",
        options,
        7,
        (labeled_component_t){"unit", "flat 14"},
        (labeled_component_t){"house", "ziggurat building"},
        (labeled_component_t){"house_number", "60-66"},
        (labeled_component_t){"road", "saffron hill"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"postcode", "ec1n 8qx"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Flat 18, Da Vinci House, 44 Saffron Hill, London, EC1N 8FH, United Kingdom",
        options,
        7,
        (labeled_component_t){"unit", "flat 18"},
        (labeled_component_t){"house", "da vinci house"},
        (labeled_component_t){"house_number", "44"},
        (labeled_component_t){"road", "saffron hill"},
        (labeled_component_t){"city", "london"},
        (labeled_component_t){"postcode", "ec1n 8fh"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "22B Derwent Parade, South Ockendon RM15 5EE, United Kingdom",
        options,
        5,
        (labeled_component_t){"house_number", "22b"},
        (labeled_component_t){"road", "derwent parade"},
        (labeled_component_t){"city", "south ockendon"},
        (labeled_component_t){"postcode", "rm15 5ee"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Unit with no house number
        "Unit 26 Roper Close, Canterbury, CT2 7EP",
        options,
        4,
        (labeled_component_t){"unit", "unit 26"},
        (labeled_component_t){"road", "roper close"},
        (labeled_component_t){"city", "canterbury"},
        (labeled_component_t){"postcode", "ct2 7ep"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Strange road name
        "Lorem House, The Marina, Lowestoft NR32 1HH, United Kingdom",
        options,
        5,
        (labeled_component_t){"house", "lorem house"},
        (labeled_component_t){"road", "the marina"},
        (labeled_component_t){"city", "lowestoft"},
        (labeled_component_t){"postcode", "nr32 1hh"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "St Johns Centre, Rope Walk, Bedford, Bedfordshire, MK42 0XE, United Kingdom",
        options,
        6,
        (labeled_component_t){"house", "st johns centre"},
        (labeled_component_t){"road", "rope walk"},
        (labeled_component_t){"city", "bedford"},
        (labeled_component_t){"state_district", "bedfordshire"},
        (labeled_component_t){"postcode", "mk42 0xe"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "St Johns Centre, 8 Rope Walk, Bedford, Bedfordshire, MK42 0XE, United Kingdom",
        options,
        7,
        (labeled_component_t){"house", "st johns centre"},
        (labeled_component_t){"house_number", "8"},
        (labeled_component_t){"road", "rope walk"},
        (labeled_component_t){"city", "bedford"},
        (labeled_component_t){"state_district", "bedfordshire"},
        (labeled_component_t){"postcode", "mk42 0xe"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Studio might be a unit, may change this later
        "Studio J, 4th Floor,,8 Lower Ormond St, Manchester M1 5QF, United Kingdom",
        options,
        7,
        (labeled_component_t){"house", "studio j"},
        (labeled_component_t){"level", "4th floor"},
        (labeled_component_t){"house_number", "8"},
        (labeled_component_t){"road", "lower ormond st"},
        (labeled_component_t){"city", "manchester"},
        (labeled_component_t){"postcode", "m1 5qf"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Victoria Institute, The Blvd, ST6 6BD, United Kingdom",
        options,
        4,
        (labeled_component_t){"house", "victoria institute"},
        (labeled_component_t){"road", "the blvd"},
        (labeled_component_t){"postcode", "st6 6bd"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "29 Lottbridge Drove, Eastbourne, East Sussex BN23 6QD",
        options,
        5,
        (labeled_component_t){"house_number", "29"},
        (labeled_component_t){"road", "lottbridge drove"},
        (labeled_component_t){"city", "eastbourne"},
        (labeled_component_t){"state_district", "east sussex"},
        (labeled_component_t){"postcode", "bn23 6qd"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Stoke-on-Trent, United Kingdom",
        options,
        2,
        (labeled_component_t){"city", "stoke-on-trent"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "The Rushes, Loughborough, Leicestershire LE11 5BG, United Kingdom",
        options,
        5,
        (labeled_component_t){"road", "the rushes"},
        (labeled_component_t){"city", "loughborough"},
        (labeled_component_t){"state_district", "leicestershire"},
        (labeled_component_t){"postcode", "le11 5bg"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "The Old Manor, 11-12 Sparrow Hill, Loughborough LE11 1BT, United Kingdom",
        options,
        6,
        (labeled_component_t){"house", "the old manor"},
        (labeled_component_t){"house_number", "11-12"},
        (labeled_component_t){"road", "sparrow hill"},
        (labeled_component_t){"city", "loughborough"},
        (labeled_component_t){"postcode", "le11 1bt"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Stockwell Head, Hinckley LE10 1RD, United Kingdom",
        options,
        4,
        (labeled_component_t){"road", "stockwell head"},
        (labeled_component_t){"city", "hinckley"},
        (labeled_component_t){"postcode", "le10 1rd"},
        (labeled_component_t){"country", "united kingdom"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Admiral Retail Park Lottbridge Drove, Eastbourne, East Sussex BN23 6QD",
        options,
        5,
        (labeled_component_t){"house", "admiral retail park"},
        (labeled_component_t){"road", "lottbridge drove"},
        (labeled_component_t){"city", "eastbourne"},
        (labeled_component_t){"state_district", "east sussex"},
        (labeled_component_t){"postcode", "bn23 6qd"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // odd structure, county abbreviation
        "12 Newgate Shopping Centre, George St, Bishop Auckland, Co. Durham, DL14 7JQ",
        options,
        6,
        (labeled_component_t){"house_number", "12"},
        (labeled_component_t){"house", "newgate shopping centre"},
        (labeled_component_t){"road", "george st"},
        (labeled_component_t){"city", "bishop auckland"},
        (labeled_component_t){"state_district", "co. durham"},
        (labeled_component_t){"postcode", "dl14 7jq"}
    ));

    CHECK_CALL(test_parse_result_equals(
        "Castle Court Shopping Centre Castle Street Caerphilly CF83 1NY",
        options,
        4,
        (labeled_component_t){"house", "castle court shopping centre"},
        (labeled_component_t){"road", "castle street"},
        (labeled_component_t){"city", "caerphilly"},
        (labeled_component_t){"postcode", "cf83 1ny"}
    ));

    PASS();
}

TEST test_im_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Multiple house names
        "Lloyds Bank International Limited, PO Box 111, Peveril Buildings, Peveril Square, Douglas, Isle of Man IM99 1JJ",
        options,
        7,
        (labeled_component_t){"house", "lloyds bank international limited"},
        (labeled_component_t){"po_box", "po box 111"},
        (labeled_component_t){"house", "peveril buildings"},
        (labeled_component_t){"road", "peveril square"},
        (labeled_component_t){"city", "douglas"},
        (labeled_component_t){"country", "isle of man"},
        (labeled_component_t){"postcode", "im99 1jj"}
    ));

    PASS();
}

TEST test_nz_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();
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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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

    PASS();
}

TEST test_co_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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


TEST test_br_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Brazil address with sem número (s/n) and CEP used with postal code
        "Theatro Municipal de São Paulo Pç. Ramos de Azevedo, s/n São Paulo - SP, CEP 01037-010",
        options,
        6,
        (labeled_component_t){"house", "theatro municipal de são paulo"},
        (labeled_component_t){"road", "pç. ramos de azevedo"},
        (labeled_component_t){"house_number", "s/n"},
        (labeled_component_t){"city", "são paulo"},
        (labeled_component_t){"state", "sp"},
        (labeled_component_t){"postcode", "cep 01037-010"}
    ));

    PASS();
}

TEST test_cn_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Example of a Kanji address
        "〒601-8446京都市西九条高畠町25-1京都醸造株式会社",
        options,
        5,
        (labeled_component_t){"postcode", "〒601-8446"},
        (labeled_component_t){"city", "京都市"},
        (labeled_component_t){"suburb", "西九条高畠町"},
        (labeled_component_t){"house_number", "25-1"},
        (labeled_component_t){"house", "京都醸造株式会社"}
    ));

    CHECK_CALL(test_parse_result_equals(
        // Ban-go style house number, level and unit
        "日本〒113-0001文京区4丁目3番2号3階323号室",
        options,
        7,
        (labeled_component_t){"country", "日本"},
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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();
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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // From: https://github.com/openvenues/libpostal/issues/39#issuecomment-221027220
        "Sars gate 2A, 562 OSLO",
        options,
        4,
        (labeled_component_t){"road", "sars gate"},
        (labeled_component_t){"house_number", "2a"},
        (labeled_component_t){"postcode", "562"},
        (labeled_component_t){"city", "oslo"}
    ));

    PASS();
}

TEST test_se_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Uses the "en trappa upp" (one floor up) form in Swedish addresses
        "Storgatan 1, 1 trappa upp, 112 01 Stockholm Sweden",
        options,
        6,
        (labeled_component_t){"road", "storgatan"},
        (labeled_component_t){"house_number", "1"},
        (labeled_component_t){"level", "1 trappa upp"},
        (labeled_component_t){"postcode", "112 01"},
        (labeled_component_t){"city", "stockholm"},
        (labeled_component_t){"country", "sweden"}
    ));
    PASS();
}

TEST test_hu_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Hungarian, 4-digit postal code
        "Szimpla Kert 1075 Budapest kazinczy utca, 14",
        options,
        5,
        (labeled_component_t){"house", "szimpla kert"},
        (labeled_component_t){"postcode", "1075"},
        (labeled_component_t){"city", "budapest"},
        (labeled_component_t){"road", "kazinczy utca"},
        (labeled_component_t){"house_number", "14"}
    ));
    PASS();
}

TEST test_ro_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

    CHECK_CALL(test_parse_result_equals(
        // Romanian address with staircase
        "str. Pacienței, nr. 9 sc. M et. 7 ap. 96 Brașov, 505722 România",
        options,
        8,
        (labeled_component_t){"road", "str. pacienței"},
        (labeled_component_t){"house_number", "nr. 9"},
        (labeled_component_t){"staircase", "sc. m"},
        (labeled_component_t){"level", "et. 7"},
        (labeled_component_t){"unit", "ap. 96"},
        (labeled_component_t){"city", "brașov"},
        (labeled_component_t){"postcode", "505722"},
        (labeled_component_t){"country", "românia"}
    ));
    PASS();
}


TEST test_ru_parses(void) {
    libpostal_address_parser_options_t options = libpostal_get_address_parser_default_options();

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

    CHECK_CALL(test_parse_result_equals(
        // Uses genitive place names, see https://github.com/openvenues/libpostal/issues/125#issuecomment-269438636
        "188541, г. Сосновый Бор Ленинградской области",
        options,
        3,
        (labeled_component_t){"postcode", "188541"},
        (labeled_component_t){"city", "г. сосновый бор"},
        (labeled_component_t){"state", "ленинградской области"}
    ));

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
    RUN_TEST(test_im_parses);
    RUN_TEST(test_nz_parses);
    RUN_TEST(test_fr_parses);
    RUN_TEST(test_es_parses);
    RUN_TEST(test_co_parses);
    RUN_TEST(test_mx_parses);
    RUN_TEST(test_br_parses);
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
    RUN_TEST(test_ro_parses);
    RUN_TEST(test_ru_parses);

    libpostal_teardown();
    libpostal_teardown_parser();
}
