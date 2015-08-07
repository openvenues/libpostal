#ifndef MSGPACK_UTILS_H
#define MSGPACK_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmp/cmp.h"
#include "collections.h"
#include "string_utils.h"

typedef struct msgpack_buffer {
    char_array *data;
    int offset;
} msgpack_buffer_t;

bool msgpack_bytes_reader(cmp_ctx_t *ctx, void *data, size_t size);
size_t msgpack_bytes_writer(cmp_ctx_t *ctx, const void *data, size_t count);

bool cmp_read_str_size_or_nil(cmp_ctx_t *ctx, char_array **str, uint32_t *size);
bool cmp_write_str_or_nil(cmp_ctx_t *ctx, char_array *str);

bool cmp_read_uint_vector(cmp_ctx_t *ctx, uint32_array **array);
bool cmp_write_uint_vector(cmp_ctx_t *ctx, uint32_array *array);

#endif