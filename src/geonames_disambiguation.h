#ifndef GEONAMES_DISAMBIGUATION_H
#define GEONAMES_DISAMBIGUATION_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "geohash/geohash.h"
#include "features.h"
#include "string_utils.h"

#define PLACE_NAME_FEATURES_DEFAULT_LENGTH 128
#define GEO_FEATURES_DEFAULT_LENGTH 720
#define POSTAL_CODE_FEATURES_DEFAULT_LENGTH 32

// Both place names and postal codes
bool geodisambig_add_name_feature(cstring_array *features, char *name);
bool geodisambig_add_country_feature(cstring_array *features, char *name, char *country);

// Only place names
bool geodisambig_add_admin1_feature(cstring_array *features, char *name, uint32_t admin1_id);
bool geodisambig_add_admin2_feature(cstring_array *features, char *name, uint32_t admin2_id);
bool geodisambig_add_geo_features(cstring_array *features, char *name, double latitude, double longitude);

#endif