#include <math.h>

#include "bloom.h"
#include "murmur/murmur.h"

#define BLOOM_FILTER_SIGNATURE 0xBABABABA

#define LOG2_SQUARED 0.4804530139182014
#define LOG2 0.6931471805599453

static int bloom_filter_check_add(bloom_filter_t *self, const char *key, size_t len, bool add) {
    uint64_t checksum[2];

    MurmurHash3_x64_128(key, len, SALT_CONSTANT, checksum);

    /* Only  calls the actual hash function once but effectively
       creates K hash functions. */
    uint64_t h;
    uint64_t h1 = checksum[0];
    uint64_t h2 = checksum[1];
    uint64_t byte;
    uint64_t mask;
    uint64_t num_bits = self->num_bits;
    unsigned char c;

    uint64_t hits = 0;

    for (int i = 0; i < self->num_hashes; i++) {
        h = (h1 + i * h2) % num_bits;
        byte = h >> 3;
        c = self->filter[byte];

        mask = 1 << (h % 8);

        if (c & mask) {
            hits++;
        } else if (add) {
            self->filter[byte] = c | mask;
        }
    }

    if (hits == self->num_hashes) {
        return 1;
    }

    return 0;

}

int bloom_filter_check(bloom_filter_t *self, const char *key, size_t len) {
    return bloom_filter_check_add(self, key, len, false);
}

int bloom_filter_add(bloom_filter_t *self, const char *key, size_t len) {
    return bloom_filter_check_add(self, key, len, true);
}

bloom_filter_t *bloom_filter_new(uint64_t capacity, double error) {
    bloom_filter_t *bloom = calloc(1, sizeof(bloom_filter_t));

    if (bloom == NULL) {
        return NULL;
    }

    bloom->ready = false;

    if (capacity < 1 || error < 0.0) {
        goto exit_free_bloom;
    }

    bloom->capacity = capacity;
    bloom->error = error;

    bloom->bits_per_entry = -(log(error) / LOG2_SQUARED);

    bloom->num_bits = (uint64_t)((double)capacity * bloom->bits_per_entry);\
    bloom->num_bytes = (uint64_t)(ceil((double)bloom->num_bits / 8));

    bloom->num_hashes = (uint32_t)ceil(LOG2 * bloom->bits_per_entry);

    // Using calloc to zero it out
    bloom->filter = calloc(bloom->num_bytes, sizeof(char));
    if (bloom->filter == NULL) {
        goto exit_free_bloom;
    }

    bloom->ready = true;

    return bloom;

exit_free_bloom:
    free(bloom);
    return NULL;
}

bool bloom_filter_write(bloom_filter_t *self, FILE *f) {
    if (!file_write_uint64(f, BLOOM_FILTER_SIGNATURE)) {
        return false;
    }

    if (!file_write_uint64(f, self->capacity)) {
        return false;
    }

    if (!file_write_double(f, self->error)) {
        return false;
    }

    if (!file_write_uint64(f, self->num_bits)) {
        return false;
    }

    if (!file_write_uint64(f, self->num_bytes)) {
        return false;
    }

    if (!file_write_uint32(f, self->num_hashes)) {
        return false;
    }

    if (!file_write_double(f, self->bits_per_entry)) {
        return false;
    }

    if (!file_write_chars(f, (char *)self->filter, self->num_bytes)) {
        return false;
    }

    return true;
}



bool bloom_filter_save(bloom_filter_t *self, char *path) {
    FILE *f;
    if ((f = fopen(path, "wb")) == NULL) {
        return false;
    }
    bool status = bloom_filter_write(self, f);
    fclose(f);
    return status;
}

bloom_filter_t *bloom_filter_read(FILE *f) {
    bloom_filter_t *bloom = malloc(sizeof(bloom_filter_t));

    if (bloom == NULL) {
        return NULL;
    }

    bloom->ready = false;

    uint64_t signature = 0;

    if (!file_read_uint64(f, &signature) || signature != BLOOM_FILTER_SIGNATURE) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_uint64(f, &bloom->capacity)) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_double(f, &bloom->error)) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_uint64(f, &bloom->num_bits)) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_uint64(f, &bloom->num_bytes)) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_uint32(f, &bloom->num_hashes)) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_double(f, &bloom->bits_per_entry)) {
        goto exit_bloom_filter_created;
    }

    bloom->filter = calloc(bloom->num_bytes, sizeof(char));
    if (bloom->filter == NULL) {
        goto exit_bloom_filter_created;
    }

    if (!file_read_chars(f, (char *)bloom->filter, bloom->num_bytes)) {
        goto exit_bloom_filter_created;
    }

    bloom->ready = true;

    return bloom;

exit_bloom_filter_created:
    bloom_filter_destroy(bloom);
    bloom = NULL;
    return bloom;
}


bloom_filter_t *bloom_filter_load(char *path) {
    FILE *f;

    if ((f = fopen(path, "rb")) == NULL) {
        return NULL;
    }

    bloom_filter_t *bloom = bloom_filter_read(f);
    fclose(f);
    return bloom;
}

void bloom_filter_destroy(bloom_filter_t *self) {
    if (self == NULL) return;

    if (self->filter != NULL) {
        free(self->filter);
    }

    free(self);
}
