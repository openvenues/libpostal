#include "geodb.h"

static geodb_t *db = NULL;

geodb_t *get_geodb(void) {
    return db;
}

void geodb_destroy(geodb_t *self) {
    if (self == NULL) return;

    if (self->trie != NULL) {
        trie_destroy(self->trie);
    }

    if (self->bloom_filter != NULL) {
        bloom_filter_destroy(self->bloom_filter);
    }

    if (self->hash_reader != NULL) {
        sparkey_hash_close(&self->hash_reader);
    }

    if (self->log_iter != NULL) {
        sparkey_logiter_close(&self->log_iter);
    }

    if (self->value_buf != NULL) {
        char_array_destroy(self->value_buf);
    }

    if (self->geoname != NULL) {
        geoname_destroy(self->geoname);
    }

    if (self->postal_code != NULL) {
        gn_postal_code_destroy(self->postal_code);
    }

    free(self);
}

geodb_t *geodb_init(char *dir) {
    if (dir == NULL) return NULL;

    geodb_t *gdb = malloc(sizeof(geodb_t));

    if (gdb == NULL) return NULL;

    char_array *path = char_array_new_size(strlen(dir));
    char_array_cat(path, dir);
    char_array_cat(path, PATH_SEPARATOR);
    char_array_cat(path, GEODB_TRIE_FILENAME);

    char *trie_path = char_array_get_string(path);

    gdb->trie = trie_load(trie_path);
    if (gdb->trie == NULL) {
        goto exit_geodb_created;
    }

    char_array_clear(path);

    char_array_cat(path, dir);
    char_array_cat(path, PATH_SEPARATOR);
    char_array_cat(path, GEODB_BLOOM_FILTER_FILENAME);

    char *bloom_path = char_array_get_string(path);

    gdb->bloom_filter = bloom_filter_load(bloom_path);
    if(gdb->bloom_filter == NULL) {
        goto exit_geodb_created;
    }

    char_array_clear(path);

    char_array_cat(path, dir);
    char_array_cat(path, PATH_SEPARATOR);
    char_array_cat(path, GEODB_HASH_FILENAME);

    char *hash_file_path = strdup(char_array_get_string(path));

    char_array_clear(path);

    char_array_cat(path, dir);
    char_array_cat(path, PATH_SEPARATOR);
    char_array_cat(path, GEODB_LOG_FILENAME);

    char *log_path = char_array_get_string(path);

    gdb->hash_reader = NULL;

    if ((sparkey_hash_open(&gdb->hash_reader, hash_file_path, log_path)) != SPARKEY_SUCCESS) {
        free(hash_file_path);
        char_array_destroy(path);
        goto exit_geodb_created;
    }

    free(hash_file_path);
    char_array_destroy(path);

    gdb->log_iter = NULL;

    if ((sparkey_logiter_create(&gdb->log_iter, sparkey_hash_getreader(gdb->hash_reader))) != SPARKEY_SUCCESS) {
        goto exit_geodb_created;
    }

    gdb->value_buf = char_array_new_size(sparkey_logreader_maxvaluelen(sparkey_hash_getreader(gdb->hash_reader)));
    if (gdb->value_buf == NULL) {
        goto exit_geodb_created;
    }

    gdb->geoname = geoname_new();
    if (gdb->geoname == NULL) {
        goto exit_geodb_created;
    }

    gdb->postal_code = gn_postal_code_new();
    if (gdb->postal_code == NULL) {
        goto exit_geodb_created;
    }

    return gdb;

exit_geodb_created:
    geodb_destroy(gdb);
    return NULL;
}

bool geodb_load(char *dir) {
    db = geodb_init(dir);
    return (db != NULL);
}


geonames_generic_t *geodb_get_len(char *key, size_t len) {
    if (db == NULL || db->hash_reader == NULL || db->log_iter == NULL) return NULL;
    sparkey_returncode ret = sparkey_hash_get(db->hash_reader, (uint8_t *)key, len, db->log_iter);
    if (sparkey_logiter_state(db->log_iter) == SPARKEY_ITER_ACTIVE) {
        uint64_t expected_value_len = sparkey_logiter_valuelen(db->log_iter);
        uint64_t actual_value_len;
        ret = sparkey_logiter_fill_value(db->log_iter, sparkey_hash_getreader(db->hash_reader), expected_value_len, (uint8_t *)db->value_buf->a, &actual_value_len);
        if (ret == SPARKEY_SUCCESS) {
            geonames_generic_t *generic = malloc(sizeof(geonames_generic_t));
            if (geonames_generic_deserialize(&generic->type, db->geoname, db->postal_code, db->value_buf)) {
                if (generic->type == GEONAMES_PLACE) {
                    generic->geoname = db->geoname;
                } else if (generic->type == GEONAMES_POSTAL_CODE) {
                    generic->postal_code = db->postal_code;
                } else {
                    free(generic);
                    return NULL;
                }
                return generic;
            }
        }
    } 
    return NULL;
}

inline geonames_generic_t *geodb_get(char *key) {
    return geodb_get_len(key, strlen(key));
}

bool geodb_module_setup(char *dir) {
    if (db == NULL) {
        return geodb_load(dir == NULL ? LIBPOSTAL_GEODB_DIR : dir);
    }

    return false;
}


void geodb_module_teardown(void) {
    if (db != NULL) {
        geodb_destroy(db);
    }
}

