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
#define PATH_SEPERATOR   "\\"
#else
#define PATH_SEPERATOR   "/"
#endif

#define PATH_SEPERATOR_LEN strlen(PATH_SEPERATOR)

#define TAB_SEPARATOR "\t"
#define TAB_SEPARATOR_LEN strlen(TAB_SEPARATOR)

#define COMMA_SEPARATOR ","
#define COMMA_SEPARATOR_LEN strlen(COMMA_SEPARATOR)

char *file_getline(FILE * f);

bool is_relative_path(struct dirent *ent);

bool file_read_int64(FILE *file, int64_t *value);
bool file_write_int64(FILE *file, int64_t value);

bool file_read_int32(FILE *file, int32_t *value);
bool file_write_int32(FILE *file, int32_t value);

bool file_read_int16(FILE *file, int16_t *value);
bool file_write_int16(FILE *file, int16_t value);

bool file_read_int8(FILE *file, int8_t *value);
bool file_write_int8(FILE *file, int8_t value);

bool file_read_chars(FILE *file, char *buf, size_t len);
bool file_write_chars(FILE *file, const char *buf, size_t len);


#ifdef __cplusplus
}
#endif

#endif
