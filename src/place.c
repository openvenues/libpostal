#include "place.h"
#include "address_parser.h"

static inline bool is_address_text_component(char *label) {
    return (string_equals(label, ADDRESS_PARSER_LABEL_HOUSE) ||
            string_equals(label, ADDRESS_PARSER_LABEL_ROAD) ||
            string_equals(label, ADDRESS_PARSER_LABEL_METRO_STATION) ||
            string_equals(label, ADDRESS_PARSER_LABEL_SUBURB) ||
            string_equals(label, ADDRESS_PARSER_LABEL_CITY_DISTRICT) ||
            string_equals(label, ADDRESS_PARSER_LABEL_CITY) ||
            string_equals(label, ADDRESS_PARSER_LABEL_STATE_DISTRICT) ||
            string_equals(label, ADDRESS_PARSER_LABEL_ISLAND) ||
            string_equals(label, ADDRESS_PARSER_LABEL_STATE) ||
            string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY_REGION) ||
            string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY) ||
            string_equals(label, ADDRESS_PARSER_LABEL_WORLD_REGION)
            );
}

language_classifier_response_t *place_languages(size_t num_components, char **labels, char **values) {
    if (num_components == 0 || values == NULL || labels == NULL) return NULL;

    language_classifier_response_t *lang_response = NULL;
    
    char *label;
    char *value;

    size_t total_size = 0;
    for (size_t i = 0; i < num_components; i++) {
        value = values[i];
        label = labels[i];
        if (is_address_text_component(label)) {
            total_size += strlen(value);
            // extra char for spaces
            if (i < num_components - 1) {
                total_size++;
            }
        }
    }

    char_array *combined = char_array_new_size(total_size);
    if (combined == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < num_components; i++) {
        value = values[i];
        label = labels[i];
        if (is_address_text_component(label)) {
            char_array_cat(combined, value);
            if (i < num_components - 1) {
                char_array_cat(combined, " ");
            }
        }
    }

    char *combined_input = char_array_get_string(combined);

    lang_response = classify_languages(combined_input);

    char_array_destroy(combined);
    return lang_response;
}



place_t *place_new(void) {
    place_t *place = calloc(1, sizeof(place_t));
    return place;
}

void place_destroy(place_t *place) {
    if (place == NULL) return;
    free(place);
}


place_t *place_from_components(size_t num_components, char **labels, char **values) {
    if (num_components == 0 || labels == NULL || values == NULL) {
        return NULL;
    }

    place_t *place = place_new();
    if (place == NULL) return NULL;

    for (size_t i = 0; i < num_components; i++) {
        char *value = values[i];
        char *label = labels[i];
        if (string_equals(label, ADDRESS_PARSER_LABEL_ROAD)) {
            if (place->street == NULL) {
                place->street = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_HOUSE)) {
            if (place->name == NULL) {
                place->name = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_HOUSE_NUMBER)) {
            if (place->house_number == NULL) {
                place->house_number = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_POSTAL_CODE)) {
            if (place->postal_code == NULL) {
                place->postal_code = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_CITY)) {
            if (place->city == NULL) {
                place->city = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_STATE)) {
            if (place->state == NULL) {
                place->state = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY)) {
            if (place->country == NULL) {
                place->country = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_SUBURB)) {
            if (place->suburb == NULL) {
                place->suburb = value;
            }
        }  else if (string_equals(label, ADDRESS_PARSER_LABEL_CITY_DISTRICT)) {
            if (place->city_district == NULL) {
                place->city_district = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_STATE_DISTRICT)) {
            if (place->state_district == NULL) {
                place->state_district = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_COUNTRY_REGION)) {
            if (place->country_region == NULL) {
                place->country_region = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_ISLAND)) {
            if (place->island == NULL) {
                place->island = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_WORLD_REGION)) {
            if (place->world_region == NULL) {
                place->world_region = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_UNIT)) {
            if (place->unit == NULL) {
                place->unit = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_TELEPHONE)) {
            if (place->telephone == NULL) {
                place->telephone = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_WEBSITE)) {
            if (place->website == NULL) {
                place->website = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_LEVEL)) {
            if (place->level == NULL) {
                place->level = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_PO_BOX)) {
            if (place->po_box == NULL) {
                place->po_box = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_BUILDING)) {
            if (place->building == NULL) {
                place->building = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_STAIRCASE)) {
            if (place->staircase == NULL) {
                place->staircase = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_ENTRANCE)) {
            if (place->entrance == NULL) {
                place->entrance = value;
            }
        } else if (string_equals(label, ADDRESS_PARSER_LABEL_METRO_STATION)) {
            if (place->metro_station == NULL) {
                place->metro_station = value;
            }
        }
    }

    return place;
}
