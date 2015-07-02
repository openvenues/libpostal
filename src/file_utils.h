#ifndef FILE_UTILS_H
#define FILE_UTILS_H


#ifdef __cplusplus
extern "C" {
#endif

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

bool is_relative_path(struct dirent *ent);

bool file_read_uint64(FILE *file, uint64_t *value);
bool file_write_uint64(FILE *file, uint64_t value);

bool file_read_double(FILE *file, double *value);
bool file_write_double(FILE *file, double value);

bool file_read_uint32(FILE *file, uint32_t *value);
bool file_write_uint32(FILE *file, uint32_t value);

bool file_read_uint16(FILE *file, uint16_t *value);
bool file_write_uint16(FILE *file, uint16_t value);

bool file_read_uint8(FILE *file, uint8_t *value);
bool file_write_uint8(FILE *file, uint8_t value);

bool file_read_chars(FILE *file, char *buf, size_t len);
bool file_write_chars(FILE *file, const char *buf, size_t len);


#ifdef __cplusplus
}
#endif

#endif
