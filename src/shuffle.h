#ifndef HAVE_SHUFFLE_H
#define HAVE_SHUFFLE_H

#include <stdlib.h>
#include <stdbool.h>

bool shuffle_file(char *filename);
bool shuffle_file_chunked(char *filename, size_t parts);
bool shuffle_file_chunked_size(char *filename, size_t chunk_size);

#endif