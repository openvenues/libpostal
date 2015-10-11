#include "geo_disambiguation.h"

#define GEONAME_KEY_NAME_ADMIN1_ID "na1"
#define GEONAME_KEY_NAME_ADMIN2_ID "na2"
#define GEONAME_GENERIC_KEY_NAME_COUNTRY_CODE "ncc"
#define GEONAME_GENERIC_KEY_NAME_COUNTRY_ID "nci"
#define GEONAME_KEY_NAME_BOUNDARY_TYPE "nb"
#define GEONAME_KEY_NAME_LANGUAGE "nl"
#define GEONAME_KEY_NAME_GEOHASH4 "nh4"

bool geodisambig_add_name_feature(cstring_array *features, char *name) {
    if (name == NULL || strlen(name) == 0) return false;
    feature_array_add(features, 1, name);

    return true;
}


bool geodisambig_add_country_code_feature(cstring_array *features, char *name, char *country) {
    if (name == NULL || strlen(name) == 0 || country == NULL || strlen(country) == 0) return false;

    feature_array_add(features, 3, name, GEONAME_GENERIC_KEY_NAME_COUNTRY_CODE, country);
    
    return true;
}

bool geodisambig_add_country_id_feature(cstring_array *features, char *name, uint32_t country_id) {
    char numeric_string[INT32_MAX_STRING_SIZE];

    if (country_id != 0 && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", country_id);
    } else {
        return false;
    }

    feature_array_add(features, 3, name, GEONAME_GENERIC_KEY_NAME_COUNTRY_ID, numeric_string);

    return true;
}

bool geodisambig_add_boundary_type_feature(cstring_array *features, char *name, uint8_t boundary_type) {
    char numeric_string[INT8_MAX_STRING_SIZE];

    if (boundary_type <= NUM_BOUNDARY_TYPES && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", boundary_type);
    } else {
        return name != NULL;
    }

    feature_array_add(features, 3, name, GEONAME_KEY_NAME_BOUNDARY_TYPE, numeric_string);

    return true;
}

bool geodisambig_add_language_feature(cstring_array *features, char *name, char *lang) {
    if (name == NULL || lang == NULL) {
        return false;
    }
    if (strlen(lang) > 0) {
        feature_array_add(features, 3, name, GEONAME_KEY_NAME_LANGUAGE, lang);
    }

    return true;
}

bool geodisambig_add_admin1_feature(cstring_array *features, char *name, uint32_t admin1_id) {
    char numeric_string[INT32_MAX_STRING_SIZE];

    if (admin1_id != 0 && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", admin1_id);
    } else {
        return name != NULL;
    }

    feature_array_add(features, 3, name, GEONAME_KEY_NAME_ADMIN1_ID, numeric_string);

    return true;

}

bool geodisambig_add_admin2_feature(cstring_array *features, char *name, uint32_t admin2_id) {
    char numeric_string[INT32_MAX_STRING_SIZE];

    if (admin2_id != 0 && name != NULL) {
        size_t n = sprintf(numeric_string, "%d", admin2_id);
    } else {
        return name != NULL;
    }

    feature_array_add(features, 3, name, GEONAME_KEY_NAME_ADMIN2_ID, numeric_string);

    return true;

}

static void geodisambig_add_geo_neighbors(cstring_array *features, char *geohash, size_t geohash_size, char *feature_name, char *name) {
    size_t neighbors_size = geohash_size * 8;
    char neighbors[neighbors_size];

    int num_strings = 0;

    if (geohash_neighbors(geohash, neighbors, neighbors_size, &num_strings) == GEOHASH_OK && num_strings == 8) {
        for (int i = 0; i < num_strings; i++) {
            char *neighbor = neighbors + geohash_size * i;
            feature_array_add(features, 3, name, feature_name, neighbor);
        }
    }

}

bool geodisambig_add_geo_features(cstring_array *features, char *name, double latitude, double longitude, bool add_neighbors) {
    if (name == NULL || strlen(name) == 0) return false;

    size_t geohash_size = 6;
    char geohash[geohash_size];

    int ret = geohash_encode(latitude, longitude, geohash, geohash_size);
    if (ret == GEOHASH_OK) {
        feature_array_add(features, 3, name, GEONAME_KEY_NAME_GEOHASH4, geohash);

        if (add_neighbors) {
            geodisambig_add_geo_neighbors(features, geohash, geohash_size, GEONAME_KEY_NAME_GEOHASH4, name);
        }
    } else {
        return false;
    }

    return true;

}

bool geodisambig_add_geoname_features(cstring_array *features, geoname_t *geoname) {
    if (geoname == NULL) return false;

    char *name = char_array_get_string(geoname->name);
    char *lang = char_array_get_string(geoname->iso_language);
    bool add_language = strlen(lang) != 0 && strcmp(lang, "abbr") != 0;

    char country_code_normalized[3];
    char *country_code = char_array_get_string(geoname->country_code);
    size_t country_code_len = strlen(country_code);
    bool add_country = country_code_len == 2;

    if (add_country) {
        strcpy(country_code_normalized, country_code);
        string_lower(country_code_normalized);
    }

    // Don't add neighbors at index time, only for queries
    bool add_neighbors = false;

    return (geodisambig_add_name_feature(features, name)
            && (!add_country || geodisambig_add_country_code_feature(features, name, country_code_normalized))
            && geodisambig_add_country_id_feature(features, name, geoname->country_geonames_id)
            && (geoname->admin1_geonames_id == 0 || geodisambig_add_admin1_feature(features, name, geoname->admin1_geonames_id))
            && (geoname->admin2_geonames_id == 0 || geodisambig_add_admin2_feature(features, name, geoname->admin2_geonames_id))
            && (geodisambig_add_boundary_type_feature(features, name, geoname->type))
            && (!add_language || geodisambig_add_language_feature(features, name, lang))
            && (geodisambig_add_geo_features(features, name, geoname->latitude, geoname->longitude, add_neighbors))
            );
}

bool geodisambig_add_postal_code_features(cstring_array *features, gn_postal_code_t *postal_code) {
    if (postal_code == NULL) return false;

    char *code = char_array_get_string(postal_code->postal_code);
    return (geodisambig_add_name_feature(features, code)
            && geodisambig_add_country_code_feature(features, code, char_array_get_string(postal_code->country_code))
            && geodisambig_add_country_id_feature(features, code, postal_code->country_geonames_id)
            );
}

