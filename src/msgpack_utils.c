#include "msgpack_utils.h"

static char_array *read_string(char_array *str, size_t len, msgpack_buffer_t *buffer) {
    char_array_clear(str);
    char_array_cat_len(str, buffer->data->a + buffer->offset, len);
    buffer->offset += len;
    return str;
}

bool msgpack_bytes_reader(cmp_ctx_t *ctx, void *data, size_t size) {
    msgpack_buffer_t *buffer = ctx->buf;
    if (buffer->offset + size > char_array_len(buffer->data)) {
        return false;
    }
    memcpy(data, buffer->data->a + buffer->offset, size);
    buffer->offset += size;
    return true;
}

size_t msgpack_bytes_writer(cmp_ctx_t *ctx, const void *data, size_t count) {
    msgpack_buffer_t *buffer = (msgpack_buffer_t *)ctx->buf;
    char_array_cat_len(buffer->data, (char *)data, count);
    buffer->offset += count;
    return count;
}

bool cmp_read_str_size_or_nil(cmp_ctx_t *ctx, char_array **str, uint32_t *size) {
    cmp_object_t obj;

    if (!cmp_read_object(ctx, &obj)) {
        return false;
    }

    if (cmp_object_is_str(&obj)) {
        *size = obj.as.str_size;
        *str = read_string(*str, *size, ctx->buf);
    } else if (cmp_object_is_nil(&obj)) {
        *size = 0;
        char_array_clear(*str);
    }

    return true;
}

bool cmp_write_str_or_nil(cmp_ctx_t *ctx, char_array *str) {
    if (str != NULL && char_array_len(str) > 0) {
        return cmp_write_str(ctx, str->a, char_array_len(str));
    } else {
        return cmp_write_nil(ctx);
    }
}

bool cmp_write_uint_vector(cmp_ctx_t *ctx, uint32_array *array) {
    size_t n = array->n;
    if (!cmp_write_array(ctx, n)) {
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        if (!cmp_write_uint(ctx, array->a[i])) {
            return false;
        }
    }
    return true;
}

bool cmp_read_uint_vector(cmp_ctx_t *ctx, uint32_array **array) {
    cmp_object_t obj;

    if (!cmp_read_object(ctx, &obj)) {
        return false;
    }

    if (!cmp_object_is_array(&obj)) {
        return false;
    }

    uint32_t size = 0;
    if (!cmp_read_array(ctx, &size)) {
        return false;
    }

    if (size == 0) {
        *array = NULL;
        return true;
    }

    uint32_array *tmp = uint32_array_new_size((size_t)size);
    uint32_t value;
    for (int i = 0; i < size; i++) {
        if (!cmp_read_uint(ctx, &value)) {
            uint32_array_destroy(tmp);
            return false;
        }
        uint32_array_push(tmp, value);
    }
    *array = tmp;
    return true;
}