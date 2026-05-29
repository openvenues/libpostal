#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#include "libpostal_config.h"
#include "string_utils.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)

#else

#define dirent direct
#define NAMLEN(dirent) ((dirent)->d_namlen)

#ifdef HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif

#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif

#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif

#endif

#ifndef BUFSIZ
#define BUFSIZ 4096
#endif

#ifdef _WIN32
#define PATH_SEPARATOR   "\\"
#else
#define PATH_SEPARATOR   "/"
#endif

#define PATH_SEPARATOR_LEN strlen(PATH_SEPARATOR)

#define TAB_SEPARATOR "\t"
#define TAB_SEPARATOR_LEN strlen(TAB_SEPARATOR)

#define COMMA_SEPARATOR ","
#define COMMA_SEPARATOR_LEN strlen(COMMA_SEPARATOR)

char *file_getline(FILE * f);

bool file_exists(char *filename);

bool is_relative_path(struct dirent *ent);

char *path_join(int n, ...);
char *path_vjoin(int n, va_list args);

uint64_t file_deserialize_uint64(unsigned char *buf);
bool file_read_uint64(FILE *file, uint64_t *value);
bool file_write_uint64(FILE *file, uint64_t value);

bool file_read_uint64_array(FILE *file, uint64_t *value, size_t n);

bool file_read_float(FILE *file, float *value);
bool file_write_float(FILE *file, float value);

bool file_read_float_array(FILE *file, float *value, size_t n);

bool file_read_double(FILE *file, double *value);
bool file_write_double(FILE *file, double value);

bool file_read_double_array(FILE *file, double *value, size_t n);

uint32_t file_deserialize_uint32(unsigned char *buf);
bool file_read_uint32(FILE *file, uint32_t *value);
bool file_write_uint32(FILE *file, uint32_t value);

bool file_read_uint32_array(FILE *file, uint32_t *value, size_t n);

uint16_t file_deserialize_uint16(unsigned char *buf);
bool file_read_uint16(FILE *file, uint16_t *value);
bool file_write_uint16(FILE *file, uint16_t value);

bool file_read_uint8(FILE *file, uint8_t *value);
bool file_write_uint8(FILE *file, uint8_t value);

bool file_read_chars(FILE *file, char *buf, size_t len);
bool file_write_chars(FILE *file, const char *buf, size_t len);

/*
 * Optional memory-mapped "flat array" cache (POSIX only)
 * ------------------------------------------------------
 * Several libpostal structures are, on disk, a small fixed header plus a few
 * flat arrays stored big-endian (e.g. CSR sparse matrices and double-array
 * tries). That layout can't be mmap'd directly on little-endian hosts, so these
 * helpers build a host-endian, 8-byte-aligned sidecar cache once and then mmap
 * it read-only, letting the arrays be shared across processes and demand-paged
 * (used and reclaimed on demand) instead of eagerly read into private memory.
 *
 * Disabled by default; controlled by the LIBPOSTAL_MMAP_CACHE environment var:
 *   unset / "0" / "off" / "false"  -> off (no behavior change, no cache files)
 *   "1" / "on" / "true"            -> on, validate cache freshness via the
 *                                     source size + mtime + inode
 *   "trust"                        -> on, but skip the mtime/inode freshness
 *                                     check (for immutable, pre-built, read-only
 *                                     deployments where those differ from build
 *                                     time, e.g. a container image)
 */

#include <sys/stat.h>

#define FILE_MMAP_CACHE_MAX_ARRAYS 4U

// Identity of the source file region the cache was built from.
typedef struct {
    uint64_t size;
    uint64_t mtime;
    uint64_t mtime_nsec;
    uint64_t ino;
} file_mmap_src_id_t;

// One array to write into the cache.
typedef struct {
    const void *data;
    uint32_t elem_size;
    uint64_t count;
} file_mmap_array_t;

// One array resolved from a mapped cache (base points into the mapping).
typedef struct {
    uint32_t elem_size;   // in: expected element size; validated against the cache
    void *base;           // out
    uint64_t count;       // out
} file_mmap_array_ref_t;

// Whether caching is enabled at all (LIBPOSTAL_MMAP_CACHE not off).
bool file_mmap_cache_enabled(void);
// Whether the freshness check should be skipped (LIBPOSTAL_MMAP_CACHE=trust).
bool file_mmap_cache_trusted(void);

file_mmap_src_id_t file_mmap_src_id_from_stat(const struct stat *st);
// Returns a malloc'd "<path><suffix>" (off==0) or "<path>.<off><suffix>" path.
char *file_mmap_cache_path(const char *path, uint64_t off, const char *suffix);

// Build a cache file atomically (temp + fsync + rename). Returns true on success.
bool file_mmap_cache_build(const char *cache_path, uint32_t magic, uint32_t version,
                           file_mmap_src_id_t src, uint64_t src_offset, uint64_t src_disk_size,
                           const void *type_header, uint32_t type_header_len,
                           const file_mmap_array_t *arrays, uint32_t num_arrays);

// Map + validate a cache. Returns the mapping base (free with file_mmap_cache_close)
// or NULL. On success fills *type_header (into the mapping), *src_disk_size, *map_len,
// and each arrays[i].base/count. arrays[i].elem_size must be set by the caller.
void *file_mmap_cache_open(const char *cache_path, uint32_t magic, uint32_t version,
                           file_mmap_src_id_t src, uint64_t src_offset,
                           const void **type_header, uint32_t type_header_len,
                           file_mmap_array_ref_t *arrays, uint32_t num_arrays,
                           uint64_t *src_disk_size, size_t *map_len);

void file_mmap_cache_close(void *base, size_t map_len);

#endif

