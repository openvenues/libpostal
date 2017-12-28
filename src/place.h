#ifndef PLACE_H
#define PLACE_H

#include <stdio.h>
#include <stdlib.h>

#include "libpostal.h"
#include "language_classifier.h"

typedef struct place {
    char *name;
    char *house_number;
    char *street;
    char *building;
    char *entrance;
    char *staircase;
    char *level;
    char *unit;
    char *po_box;
    char *metro_station;
    char *suburb;
    char *city_district;
    char *city;
    char *state_district;
    char *island;
    char *state;
    char *country_region;
    char *country;
    char *world_region;
    char *postal_code;
    char *telephone;
    char *website;
} place_t;

language_classifier_response_t *place_languages(size_t num_components, char **labels, char **values);

place_t *place_new(void);

place_t *place_from_components(size_t num_components, char **labels, char **values);

void place_destroy(place_t *place);

#endif