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

bool is_relative_path(struct dirent *ent) {
    return strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0;
}

bool file_read_int64(FILE *file, int64_t *value) {
    unsigned char buf[8];

    if (fread(buf, 8, 1, file) == 1) {
        *value = ((uint64_t)buf[0] << 56) | 
                 ((uint64_t)buf[1] << 48) |
                 ((uint64_t)buf[2] << 40) |
                 ((uint64_t)buf[3] << 32) |
                 ((uint64_t)buf[4] << 24) |
                 ((uint64_t)buf[5] << 16) |
                 ((uint64_t)buf[6] << 8) |
                  (uint64_t)buf[7];
        return true;
    }
    return false;
}

bool file_write_int64(FILE *file, int64_t value) {
    unsigned char buf[8];
    buf[0] = ((value >> 56) & 0xff);
    buf[1] = ((value >> 48) & 0xff);
    buf[2] = ((value >> 40) & 0xff);
    buf[3] = ((value >> 32) & 0xff);
    buf[4] = ((value >> 24) & 0xff);
    buf[5] = ((value >> 16) & 0xff);
    buf[6] = ((value >> 8) & 0xff);
    buf[7] = value & 0xff;

    return (fwrite(buf, 8, 1, file) == 1);
}

bool file_read_int32(FILE *file, int32_t *value) {
    unsigned char buf[4];

    if (fread(buf, 4, 1, file) == 1) {
        *value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        return true;
    }
    return false;
}

bool file_write_int32(FILE *file, int32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xff;
    buf[1] = (value >> 16) & 0xff;
    buf[2] = (value >> 8) & 0xff;
    buf[3] = value & 0xff;

    return (fwrite(buf, 4, 1, file) == 1);
}

bool file_read_int16(FILE *file, int16_t *value) {
    unsigned char buf[2];

    if (fread(buf, 2, 1, file) == 1) {
        *value = (buf[0] << 8) | buf[1];
        return true;
    }
    return false;

}

bool file_write_int16(FILE *file, int16_t value) {
    unsigned char buf[2];

    buf[0] = value >> 8;
    buf[1] = value & 0xff;

    return (fwrite(buf, 2, 1, file) == 1);
}

bool file_read_int8(FILE *file, int8_t *value) {
    return (fread(value, sizeof(int8_t), 1, file) == 1);
}

bool file_write_int8(FILE *file, int8_t value) {
    return (fwrite(&value, sizeof(int8_t), 1, file) == 1);
}

bool file_read_chars(FILE *file, char *buf, size_t len) {
    return (fread(buf, sizeof(char), len, file) == len);
}

bool file_write_chars(FILE *file, const char *buf, size_t len) {
    return (fwrite(buf, sizeof(char), len, file) == len);
}
