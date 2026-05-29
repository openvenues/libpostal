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
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
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
    return ((uint16_t)buf[0] << 8) | buf[1];
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

#ifndef _WIN32

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__APPLE__)
#define FILE_MMAP_MTIME_NSEC(st) ((uint64_t)(st).st_mtimespec.tv_nsec)
#else
#define FILE_MMAP_MTIME_NSEC(st) ((uint64_t)(st).st_mtim.tv_nsec)
#endif

#define FILE_MMAP_ENDIAN_CHECK 0x01020304U

typedef enum {
    FILE_MMAP_MODE_OFF = 0,
    FILE_MMAP_MODE_ON = 1,
    FILE_MMAP_MODE_TRUST = 2
} file_mmap_mode_t;

static file_mmap_mode_t file_mmap_cache_mode(void) {
    const char *v = getenv("LIBPOSTAL_MMAP_CACHE");
    if (v == NULL || v[0] == '\0')
        return FILE_MMAP_MODE_OFF;
    if (strcmp(v, "trust") == 0 || strcmp(v, "TRUST") == 0)
        return FILE_MMAP_MODE_TRUST;
    if (strcmp(v, "0") == 0 || strcmp(v, "off") == 0 ||
        strcmp(v, "false") == 0 || strcmp(v, "no") == 0)
        return FILE_MMAP_MODE_OFF;
    return FILE_MMAP_MODE_ON;
}

bool file_mmap_cache_enabled(void) {
    return file_mmap_cache_mode() != FILE_MMAP_MODE_OFF;
}

bool file_mmap_cache_trusted(void) {
    return file_mmap_cache_mode() == FILE_MMAP_MODE_TRUST;
}

// Common header at the start of every cache file. 72 bytes, a multiple of 8 so
// the type header and (8-aligned) arrays that follow keep their alignment.
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t endian_check;
    uint32_t num_arrays;
    uint64_t src_size;
    uint64_t src_mtime;
    uint64_t src_mtime_nsec;
    uint64_t src_ino;
    uint64_t src_offset;
    uint64_t src_disk_size;
    uint32_t type_header_len;
    uint32_t reserved;
} file_mmap_cache_common_t;

typedef struct {
    uint32_t elem_size;
    uint32_t reserved;
    uint64_t count;
} file_mmap_array_desc_t;

static inline uint64_t file_mmap_align8(uint64_t x) {
    return (x + 7u) & ~((uint64_t)7u);
}

file_mmap_src_id_t file_mmap_src_id_from_stat(const struct stat *st) {
    file_mmap_src_id_t id;
    id.size = (uint64_t)st->st_size;
    id.mtime = (uint64_t)st->st_mtime;
    id.mtime_nsec = FILE_MMAP_MTIME_NSEC(*st);
    id.ino = (uint64_t)st->st_ino;
    return id;
}

char *file_mmap_cache_path(const char *path, uint64_t off, const char *suffix) {
    size_t len = strlen(path) + strlen(suffix) + 32;
    char *cache_path = malloc(len);
    if (cache_path == NULL)
        return NULL;
    if (off == 0)
        snprintf(cache_path, len, "%s%s", path, suffix);
    else
        snprintf(cache_path, len, "%s.%llu%s", path, (unsigned long long)off, suffix);
    return cache_path;
}

static bool file_mmap_write_all(FILE *f, const void *buf, size_t len) {
    return len == 0 || fwrite(buf, 1, len, f) == len;
}

static bool file_mmap_write_pad(FILE *f, uint64_t *offset) {
    static const char zeros[8] = {0};
    uint64_t pad = file_mmap_align8(*offset) - *offset;
    if (pad > 0 && fwrite(zeros, 1, (size_t)pad, f) != (size_t)pad)
        return false;
    *offset += pad;
    return true;
}

bool file_mmap_cache_build(const char *cache_path, uint32_t magic, uint32_t version,
                           file_mmap_src_id_t src, uint64_t src_offset, uint64_t src_disk_size,
                           const void *type_header, uint32_t type_header_len,
                           const file_mmap_array_t *arrays, uint32_t num_arrays) {
    if (num_arrays > FILE_MMAP_CACHE_MAX_ARRAYS)
        return false;

    size_t path_len = strlen(cache_path);
    char *tmp_path = malloc(path_len + 32);
    if (tmp_path == NULL)
        return false;
    snprintf(tmp_path, path_len + 32, "%s.tmp.%ld", cache_path, (long)getpid());

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        free(tmp_path);
        return false;
    }

    bool ok = false;
    uint64_t offset = 0;

    file_mmap_cache_common_t header;
    memset(&header, 0, sizeof(header));
    header.magic = magic;
    header.version = version;
    header.endian_check = FILE_MMAP_ENDIAN_CHECK;
    header.num_arrays = num_arrays;
    header.src_size = src.size;
    header.src_mtime = src.mtime;
    header.src_mtime_nsec = src.mtime_nsec;
    header.src_ino = src.ino;
    header.src_offset = src_offset;
    header.src_disk_size = src_disk_size;
    header.type_header_len = type_header_len;

    if (!file_mmap_write_all(f, &header, sizeof(header)))
        goto cleanup;
    offset += sizeof(header);

    if (!file_mmap_write_all(f, type_header, type_header_len))
        goto cleanup;
    offset += type_header_len;

    if (!file_mmap_write_pad(f, &offset))
        goto cleanup;

    for (uint32_t i = 0; i < num_arrays; i++) {
        file_mmap_array_desc_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.elem_size = arrays[i].elem_size;
        desc.count = arrays[i].count;
        if (!file_mmap_write_all(f, &desc, sizeof(desc)))
            goto cleanup;
        offset += sizeof(desc);
    }

    for (uint32_t i = 0; i < num_arrays; i++) {
        if (!file_mmap_write_pad(f, &offset))
            goto cleanup;
        size_t bytes = (size_t)arrays[i].elem_size * (size_t)arrays[i].count;
        if (!file_mmap_write_all(f, arrays[i].data, bytes))
            goto cleanup;
        offset += bytes;
    }

    if (fflush(f) != 0)
        goto cleanup;
    // Make the data durable before the rename publishes the cache.
    if (fsync(fileno(f)) != 0)
        goto cleanup;

    ok = true;

cleanup:
    fclose(f);
    if (ok && rename(tmp_path, cache_path) != 0)
        ok = false;
    if (!ok)
        unlink(tmp_path);
    free(tmp_path);
    return ok;
}

void *file_mmap_cache_open(const char *cache_path, uint32_t magic, uint32_t version,
                           file_mmap_src_id_t src, uint64_t src_offset,
                           const void **type_header, uint32_t type_header_len,
                           file_mmap_array_ref_t *arrays, uint32_t num_arrays,
                           uint64_t *src_disk_size, size_t *map_len) {
    if (num_arrays > FILE_MMAP_CACHE_MAX_ARRAYS)
        return NULL;

    int fd = open(cache_path, O_RDONLY);
    if (fd < 0)
        return NULL;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(file_mmap_cache_common_t)) {
        close(fd);
        return NULL;
    }
    size_t len = (size_t)st.st_size;

    void *base = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED)
        return NULL;

    const file_mmap_cache_common_t *h = (const file_mmap_cache_common_t *)base;
    bool trusted = file_mmap_cache_trusted();
    if (h->magic != magic ||
        h->version != version ||
        h->endian_check != FILE_MMAP_ENDIAN_CHECK ||
        h->num_arrays != num_arrays ||
        h->type_header_len != type_header_len ||
        h->src_offset != src_offset ||
        h->src_size != src.size ||
        // Freshness fields are skipped in trust mode (read-only/pre-built caches).
        (!trusted && (h->src_mtime != src.mtime ||
                      h->src_mtime_nsec != src.mtime_nsec ||
                      h->src_ino != src.ino))) {
        munmap(base, len);
        return NULL;
    }

    uint64_t off = file_mmap_align8(sizeof(file_mmap_cache_common_t) + (uint64_t)type_header_len);
    if (off + (uint64_t)num_arrays * sizeof(file_mmap_array_desc_t) > len) {
        munmap(base, len);
        return NULL;
    }
    const file_mmap_array_desc_t *descs = (const file_mmap_array_desc_t *)((const char *)base + off);
    off += (uint64_t)num_arrays * sizeof(file_mmap_array_desc_t);

    for (uint32_t i = 0; i < num_arrays; i++) {
        if (descs[i].elem_size != arrays[i].elem_size || descs[i].elem_size == 0) {
            munmap(base, len);
            return NULL;
        }
        off = file_mmap_align8(off);
        // Overflow-safe bounds check: compare count against remaining space without
        // computing elem_size*count (which could wrap 64-bit for a corrupt cache).
        if (off > len ||
            descs[i].count > (len - off) / descs[i].elem_size ||
            ((uintptr_t)((char *)base + off) & 7u) != 0) {
            munmap(base, len);
            return NULL;
        }
        uint64_t bytes = (uint64_t)descs[i].elem_size * descs[i].count;
        arrays[i].base = (char *)base + off;
        arrays[i].count = descs[i].count;
        off += bytes;
    }

    if (type_header != NULL)
        *type_header = (type_header_len > 0) ? (const char *)base + sizeof(file_mmap_cache_common_t) : NULL;
    if (src_disk_size != NULL)
        *src_disk_size = h->src_disk_size;
    if (map_len != NULL)
        *map_len = len;

    return base;
}

void file_mmap_cache_close(void *base, size_t map_len) {
    if (base != NULL)
        munmap(base, map_len);
}

#endif /* _WIN32 */
