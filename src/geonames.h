#ifndef GEONAMES_H
#define GEONAMES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "collections.h"
#include "cmp/cmp.h"
#include "features.h"
#include "string_utils.h"

typedef enum {
    COUNTRY = 0,
    ADMIN1 = 1,
    ADMIN2 = 2,
    ADMIN3 = 3,
    ADMIN4 = 4,
    ADMIN_OTHER = 5,
    LOCALITY = 6,
    NEIGHBORHOOD = 7
} boundary_type_t;

typedef struct buffer {
    char_array *data;
    int offset;
} buffer_t;

typedef struct geoname {
    uint32_t geonames_id;
    char_array *name;
    char_array *canonical;
    boundary_type_t type;
    char_array *iso_language;
    bool is_preferred_name;
    uint32_t population;
    double latitude;
    double longitude;
    char_array *feature_code;
    char_array *country_code;
    char_array *admin1_code;
    uint32_t admin1_geonames_id;
    char_array *admin2_code;
    uint32_t admin2_geonames_id;
    char_array *admin3_code;
    uint32_t admin3_geonames_id;
    char_array *admin4_code;
    uint32_t admin4_geonames_id;
} geoname_t;

/* We want to reuse objects here, so only call
 * geoname_create once or twice and populate the same
 * object repeatedly with geoname_deserialize.
 * This helps avoid making too many malloc/free calls
*/
geoname_t *geoname_new(void);
bool geoname_deserialize(geoname_t *self, char_array *str);
bool geoname_serialize(geoname_t *self, char_array *str);
void geoname_print(geoname_t *self);
void geoname_clear(geoname_t *self);
void geoname_destroy(geoname_t *self);

typedef struct gn_postal_code {
    char_array *postal_code;
    char_array *country_code;
    bool have_lat_lon;
    double latitude;
    double longitude;
    uint8_t accuracy;
    bool have_containing_geoname;
    char_array *containing_geoname;
    uint32_t containing_geonames_id;
    uint32_array *admin1_ids;
    uint32_array *admin2_ids;
    uint32_array *admin3_ids;
} gn_postal_code_t;

gn_postal_code_t *gn_postal_code_new(void);
bool gn_postal_code_deserialize(gn_postal_code_t *self, char_array *str);
bool gn_postal_code_serialize(gn_postal_code_t *self, char_array *str);
void gn_postal_code_print(gn_postal_code_t *self);
void gn_postal_code_clear(gn_postal_code_t *self);
void gn_postal_code_destroy(gn_postal_code_t *self);

typedef enum {
    GEONAMES_NAME, 
    GEONAMES_POSTAL_CODE 
} gn_type;

typedef struct geonames_generic {
    gn_type type;
    union {
        geoname_t *geoname;
        gn_postal_code_t *postal_code;
    };
} geonames_generic_t;

VECTOR_INIT(gn_generic_array, geonames_generic_t);

bool geonames_generic_serialize(geonames_generic_t *gn, char_array *str);
bool geonames_generic_deserialize(gn_type *type, geoname_t *geoname, gn_postal_code_t *postal_code, char_array *str);

#ifdef __cplusplus
}
#endif

#endif