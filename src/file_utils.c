#include "file_utils.h"

char *file_getline(FILE * f)
{
    char buf[BUFSIZ];

    char *ret = NULL;

    size_t buf_len = 0;
    size_t ret_size = 0;

    while (fgets(buf, BUFSIZ, f) != NULL) {
        buf_len = strlen(buf);
        if (buf_len == 0) break;
        ret = realloc(ret, ret_size + buf_len + 1);
        memcpy(ret+ret_size, buf, buf_len);
        ret_size += buf_len;
        ret[ret_size] = '\0';
        if (ret[ret_size - 1] == '\n') {
            ret[ret_size - 1] = '\0';
            // Handle carriage returns
            if (ret_size > 1 && ret[ret_size-2] == '\r') {
                ret[ret_size - 2] = '\0';
            }
            break;
        }
    }

    if (ret_size == 0) {
        return NULL;
    } 
    return ret;
}

bool file_exists(char *filename) {
    FILE *f = fopen(filename, "r");
    bool exists = f != NULL;
    if (exists) fclose(f);
    return exists;
}

bool is_relative_path(struct dirent *ent) {
    return strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0;
}

char *path_vjoin(int n, va_list args) {
    char_array *path = char_array_new();
    if (path == NULL) return NULL;
    char_array_add_vjoined(path, PATH_SEPARATOR, true, n, args);
    return char_array_to_string(path);
}

char *path_join(int n, ...) {
    va_list args;
    va_start(args, n);
    char *path = path_vjoin(n, args);
    va_end(args);
    return path;
}

inline uint64_t file_deserialize_uint64(unsigned char *buf) {
    return ((uint64_t)buf[0] << 56) | 
           ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) |
           ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) |
           ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8) |
            (uint64_t)buf[7];
}

bool file_read_uint64(FILE *file, uint64_t *value) {
    unsigned char buf[8];

    if (fread(buf, 8, 1, file) == 1) {
        *value = file_deserialize_uint64(buf);
        return true;
    }
    return false;
}

bool file_read_uint64_array(FILE *file, uint64_t *value, size_t n) {
    unsigned char *buf = malloc(n * sizeof(uint64_t));

    if (buf == NULL) return false;

    bool ret = false;

    if (fread(buf, sizeof(uint64_t), n, file) == n) {

        for (size_t i = 0, byte_offset = 0; i < n; i++, byte_offset += sizeof(uint64_t)) {
            unsigned char *ptr = buf + byte_offset;
            value[i] = file_deserialize_uint64(ptr);
        }
        ret = true;
    }
    free(buf);
    return ret;
}


bool file_write_uint64(FILE *file, uint64_t value) {
    unsigned char buf[8];
    buf[0] = ((uint8_t)(value >> 56) & 0xff);
    buf[1] = ((uint8_t)(value >> 48) & 0xff);
    buf[2] = ((uint8_t)(value >> 40) & 0xff);
    buf[3] = ((uint8_t)(value >> 32) & 0xff);
    buf[4] = ((uint8_t)(value >> 24) & 0xff);
    buf[5] = ((uint8_t)(value >> 16) & 0xff);
    buf[6] = ((uint8_t)(value >> 8) & 0xff);
    buf[7] = (uint8_t)(value & 0xff);

    return (fwrite(buf, 8, 1, file) == 1);
}

typedef union {
    uint64_t u;
    double d;
} uint64_double_t;

bool file_read_double(FILE *file, double *value) {
    uint64_double_t ud;
    if (!file_read_uint64(file, &ud.u)) {
        return false;
    }
    *value = ud.d;
    return true;
}

bool file_read_double_array(FILE *file, double *value, size_t n) {
    unsigned char *buf = malloc(n * sizeof(uint64_t));

    if (buf == NULL) return false;

    bool ret = false;

    if (fread(buf, sizeof(uint64_t), n, file) == n) {
        uint64_double_t ud;

        for (size_t i = 0, byte_offset = 0; i < n; i++, byte_offset += sizeof(uint64_t)) {
            unsigned char *ptr = buf + byte_offset;
            ud.u = file_deserialize_uint64(ptr);
            value[i] = ud.d;
        }
        ret = true;
    }
    free(buf);
    return ret;
}

bool file_write_double(FILE *file, double value) {
    uint64_double_t ud;
    ud.d = value;
    return file_write_uint64(file, ud.u);
}

typedef union {
    uint32_t u;
    float f;
} uint32_float_t;

bool file_read_float(FILE *file, float *value) {
    uint32_float_t uf;

    if (!file_read_uint32(file, &uf.u)) {
        return false;
    }
    *value = uf.f;
    return true;
}

bool file_read_float_array(FILE *file, float *value, size_t n) {
    unsigned char *buf = malloc(n * sizeof(uint32_t));

    if (buf == NULL) return false;

    bool ret = false;

    if (fread(buf, sizeof(uint32_t), n, file) == n) {
        uint32_float_t uf;

        for (size_t i = 0, byte_offset = 0; i < n; i++, byte_offset += sizeof(uint32_t)) {
            unsigned char *ptr = buf + byte_offset;
            uf.u = file_deserialize_uint32(ptr);
            value[i] = uf.f;
        }
        ret = true;
    }
    free(buf);
    return ret;
}

bool file_write_float(FILE *file, float value) {
    uint32_float_t uf;
    uf.f = value;
    return file_write_uint32(file, uf.u);

}

inline uint32_t file_deserialize_uint32(unsigned char *buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

bool file_read_uint32(FILE *file, uint32_t *value) {
    unsigned char buf[4];

    if (fread(buf, 4, 1, file) == 1) {
        *value = file_deserialize_uint32(buf);
        return true;
    }
    return false;
}

bool file_read_uint32_array(FILE *file, uint32_t *value, size_t n) {
    unsigned char *buf = malloc(n * sizeof(uint32_t));

    if (buf == NULL) return false;

    bool ret = false;

    if (fread(buf, sizeof(uint32_t), n, file) == n) {

        for (size_t i = 0, byte_offset = 0; i < n; i++, byte_offset += sizeof(uint32_t)) {
            unsigned char *ptr = buf + byte_offset;
            value[i] = file_deserialize_uint32(ptr);
        }
        ret = true;
    }
    free(buf);
    return ret;
}


bool file_write_uint32(FILE *file, uint32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xff;
    buf[1] = (value >> 16) & 0xff;
    buf[2] = (value >> 8) & 0xff;
    buf[3] = value & 0xff;

    return (fwrite(buf, 4, 1, file) == 1);
}


inline uint16_t file_deserialize_uint16(unsigned char *buf) {
    return (buf[0] << 8) | buf[1];
}


bool file_read_uint16(FILE *file, uint16_t *value) {
    unsigned char buf[2];

    if (fread(buf, 2, 1, file) == 1) {
        *value = file_deserialize_uint16(buf);
        return true;
    }
    return false;

}

bool file_write_uint16(FILE *file, uint16_t value) {
    unsigned char buf[2];

    buf[0] = value >> 8;
    buf[1] = value & 0xff;

    return (fwrite(buf, 2, 1, file) == 1);
}

bool file_read_uint8(FILE *file, uint8_t *value) {
    return (fread(value, sizeof(int8_t), 1, file) == 1);
}

bool file_write_uint8(FILE *file, uint8_t value) {
    return (fwrite(&value, sizeof(int8_t), 1, file) == 1);
}

bool file_read_chars(FILE *file, char *buf, size_t len) {
    return (fread(buf, sizeof(char), len, file) == len);
}

bool file_write_chars(FILE *file, const char *buf, size_t len) {
    return (fwrite(buf, sizeof(char), len, file) == len);
}
