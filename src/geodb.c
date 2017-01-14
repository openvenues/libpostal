#include "geodb.h"

static geodb_t *geodb = NULL;

geodb_t *get_geodb(void) {
    return geodb;
}

void geodb_destroy(geodb_t *self) {
    if (self == NULL) return;

    if (self->names != NULL) {
        trie_destroy(self->names);
    }

    if (self->features != NULL) {
        trie_destroy(self->features);
    }

    if (self->postal_codes != NULL) {
        cstring_array_destroy(self->postal_codes);
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

    geodb_t *gdb = calloc(1, sizeof(geodb_t));

    if (gdb == NULL) return NULL;

    char_array *path = char_array_new_size(strlen(dir));
    char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, GEODB_NAMES_TRIE_FILENAME);

    char *names_path = char_array_get_string(path);

    gdb->names = trie_load(names_path);
    if (gdb->names == NULL) {
        goto exit_geodb_created;
    }

    char_array_clear(path);

    char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, GEODB_FEATURES_TRIE_FILENAME);

    char *features_path = char_array_get_string(path);

    gdb->features = trie_load(features_path);
    if(gdb->features == NULL) {
        goto exit_geodb_created;
    }

    char_array_clear(path);

    char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, GEODB_POSTAL_CODES_FILENAME);
    char *postal_codes_path = char_array_get_string(path);

    FILE *f = fopen(postal_codes_path, "rb");

    uint64_t num_postal_strings = 0;
    if (!file_read_uint64(f, &num_postal_strings)) {
        goto exit_geodb_created;
    }

    uint64_t postal_codes_str_len;

    if (!file_read_uint64(f, &postal_codes_str_len)) {
        goto exit_geodb_created;
    }

    char_array *array = char_array_new_size((size_t)postal_codes_str_len);

    if (!file_read_chars(f, array->a, (size_t)postal_codes_str_len)) {
        goto exit_geodb_created;
    }

    array->n = (size_t)postal_codes_str_len;

    gdb->postal_codes = cstring_array_from_char_array(array);

    if (cstring_array_num_strings(gdb->postal_codes) != num_postal_strings) {
        goto exit_geodb_created;
    }

    fclose(f);
    char_array_clear(path);

    char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, GEODB_HASH_FILENAME);

    char *hash_file_path = strdup(char_array_get_string(path));

    char_array_clear(path);

    char_array_cat_joined(path, PATH_SEPARATOR, true, 2, dir, GEODB_LOG_FILENAME);

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
    geodb = geodb_init(dir);
    return (geodb != NULL);
}


bool search_geodb_with_phrases(char *str, phrase_array **phrases) {
    if (str == NULL) return false;

    return trie_search_with_phrases(geodb->names, str, phrases);
}

phrase_array *search_geodb(char *str) {
    phrase_array *phrases = NULL;

    if (!search_geodb_with_phrases(str, &phrases)) {
        return NULL;
    }   

    return phrases;
}


bool search_geodb_tokens_with_phrases(char *str, token_array *tokens, phrase_array **phrases) {
    if (str == NULL) return false;

    return trie_search_tokens_with_phrases(geodb->names, str, tokens, phrases);
}


phrase_array *search_geodb_tokens(char *str, token_array *tokens) {
    phrase_array *phrases = NULL;

    if (!search_geodb_tokens_with_phrases(str, tokens, &phrases)) {
        return NULL;
    }

    return phrases;
}


geonames_generic_t *geodb_get_len(char *key, size_t len) {
    if (geodb == NULL || geodb->hash_reader == NULL || geodb->log_iter == NULL) return NULL;
    sparkey_returncode ret = sparkey_hash_get(geodb->hash_reader, (uint8_t *)key, len, geodb->log_iter);
    if (sparkey_logiter_state(geodb->log_iter) == SPARKEY_ITER_ACTIVE) {
        uint64_t expected_value_len = sparkey_logiter_valuelen(geodb->log_iter);
        uint64_t actual_value_len;
        ret = sparkey_logiter_fill_value(geodb->log_iter, sparkey_hash_getreader(geodb->hash_reader), expected_value_len, (uint8_t *)geodb->value_buf->a, &actual_value_len);
        if (ret == SPARKEY_SUCCESS) {
            geonames_generic_t *generic = malloc(sizeof(geonames_generic_t));
            if (geonames_generic_deserialize(&generic->type, geodb->geoname, geodb->postal_code, geodb->value_buf)) {
                if (generic->type == GEONAMES_PLACE) {
                    generic->geoname = geodb->geoname;
                } else if (generic->type == GEONAMES_POSTAL_CODE) {
                    generic->postal_code = geodb->postal_code;
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
    if (geodb == NULL) {
        return geodb_load(dir == NULL ? LIBPOSTAL_GEODB_DIR : dir);
    }

    return true;
}


void geodb_module_teardown(void) {
    if (geodb != NULL) {
        geodb_destroy(geodb);
    }
    geodb = NULL;
}

