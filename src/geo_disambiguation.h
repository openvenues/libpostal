#ifndef GEO_DISAMBIGUATION_H
#define GEO_DISAMBIGUATION_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "geohash/geohash.h"
#include "features.h"
#include "geonames.h"
#include "string_utils.h"

#define PLACE_NAME_FEATURES_DEFAULT_LENGTH 128
#define GEO_FEATURES_DEFAULT_LENGTH 720
#define POSTAL_CODE_FEATURES_DEFAULT_LENGTH 32

// Both place names and postal codes
bool geodisambig_add_name_feature(cstring_array *features, char *name);
bool geodisambig_add_country_code_feature(cstring_array *features, char *name, char *country);

// Only place names
bool geodisambig_add_boundary_type_feature(cstring_array *features, char *name, uint8_t boundary_type);
bool geodisambig_add_language_feature(cstring_array *features, char *name, char *lang);
bool geodisambig_add_admin1_feature(cstring_array *features, char *name, uint32_t admin1_id);
bool geodisambig_add_admin2_feature(cstring_array *features, char *name, uint32_t admin2_id);
bool geodisambig_add_geo_features(cstring_array *features, char *name, double latitude, double longitude, bool add_neighbors);

bool geodisambig_add_geoname_features(cstring_array *features, geoname_t *geoname);
bool geodisambig_add_postal_code_features(cstring_array *features, gn_postal_code_t *postal_code);

#endif