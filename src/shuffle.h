#ifndef HAVE_SHUFFLE_H
#define HAVE_SHUFFLE_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define CHUNK_SIZE_MB UINT64_C(1024) * UINT64_C(1024)
#define CHUNK_SIZE_GB UINT64_C(1024) * (CHUNK_SIZE_MB)
#define DEFAULT_SHUFFLE_CHUNK_SIZE UINT64_C(2) * (CHUNK_SIZE_GB)

bool shuffle_file(char *filename);
bool shuffle_file_chunked(char *filename, size_t parts);
bool shuffle_file_chunked_size(char *filename, size_t chunk_size);

#endif