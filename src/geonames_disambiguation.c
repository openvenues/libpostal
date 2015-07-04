#include "geonames_disambiguation.h"

#define GEONAME_GENERIC_KEY_NAME "n"
#define GEONAME_KEY_NAME_ADMIN1_ID "na1"
#define GEONAME_KEY_NAME_ADMIN2_ID "na2"
#define GEONAME_KEY_NAME_COUNTRY "nc"
#define GEONAME_KEY_NAME_GEOHASH5 "nh5"
#define GEONAME_KEY_NAME_GEOHASH6 "nh6"
#define GEONAME_KEY_NAME_GEOHASH7 "nh7"

bool geodisambig_add_name_feature(cstring_array *features, char *name) {
    if (name == NULL || strlen(name) == 0) return false;
    feature_array_add(features, 2, GEONAME_GENERIC_KEY_NAME, name);
    return true;
}


bool geodisambig_add_country_feature(cstring_array *features, char *name, char *country) {
    if (name == NULL || strlen(name) == 0 || country == NULL || strlen(country) == 0) return false;

    feature_array_add(features, 3, GEONAME_KEY_NAME_COUNTRY, name, country);
    
    return true;
}

bool geodisambig_add_admin1_feature(cstring_array *features, char *name, uint32_t admin1_id) {
    char numeric_string[INT32_MAX_STRING_SIZE];
    printf("%d\n", admin1_id);

    if (admin1_id != 0 && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", admin1_id);
    } else {
        return false;
    }

    feature_array_add(features, 3, GEONAME_KEY_NAME_ADMIN1_ID, name, numeric_string);
    return true;

}

bool geodisambig_add_admin2_feature(cstring_array *features, char *name, uint32_t admin2_id) {
    char numeric_string[INT32_MAX_STRING_SIZE];

    if (admin2_id != 0 && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", admin2_id);
    } else {
        return false;
    }

    feature_array_add(features, 3, GEONAME_KEY_NAME_ADMIN2_ID, name, numeric_string);
    return true;

}

static void geodisambig_add_geo_neighbors(cstring_array *features, char *geohash, size_t geohash_size, char *feature_name, char *name) {
    size_t neighbors_size = geohash_size * 8;
    char neighbors[neighbors_size];

    int num_strings = 0;

    if (geohash_neighbors(geohash, neighbors, neighbors_size, &num_strings) == GEOHASH_OK && num_strings == 8) {
        for (int i = 0; i < num_strings; i++) {
            char *neighbor = neighbors + geohash_size * i;
            feature_array_add(features, 3, feature_name, name, neighbor);
        }
    }

}

bool geodisambig_add_geo_features(cstring_array *features, char *name, double latitude, double longitude) {
    if (name == NULL || strlen(name) == 0) return false;

    size_t geohash_size = 8;
    char geohash[geohash_size];

    if ((geohash_encode(latitude, longitude, geohash, geohash_size)) == GEOHASH_OK) {
        feature_array_add(features, 3, GEONAME_KEY_NAME_GEOHASH7, name, geohash);

        int num_strings = 0;

        geodisambig_add_geo_neighbors(features, geohash, geohash_size, GEONAME_KEY_NAME_GEOHASH7, name);

        geohash_size--;
        geohash[geohash_size - 1] = '\0';
        feature_array_add(features, 3, GEONAME_KEY_NAME_GEOHASH6, name, geohash);

        geodisambig_add_geo_neighbors(features, geohash, geohash_size, GEONAME_KEY_NAME_GEOHASH6, name);

        geohash_size--;
        geohash[geohash_size - 1] = '\0';

        feature_array_add(features, 3, GEONAME_KEY_NAME_GEOHASH5, name, geohash);

        geodisambig_add_geo_neighbors(features, geohash, geohash_size, GEONAME_KEY_NAME_GEOHASH5, name);
           
    } else {
        return false;
    }

    return true;

}