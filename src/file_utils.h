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

#endif

