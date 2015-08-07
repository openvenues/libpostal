#ifndef BLOOM_H
#define BLOOM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "file_utils.h"

#define SALT_CONSTANT 0x66e8c41d

typedef struct bloom_filter {
    uint64_t capacity;
    double error;
    uint64_t num_bits;
    uint64_t num_bytes;
    uint32_t num_hashes;

    double bits_per_entry;
    unsigned char *filter;
    bool ready;
} bloom_filter_t;


bloom_filter_t *bloom_filter_new(uint64_t capacity, double error);

int bloom_filter_check(bloom_filter_t *self, const char *key, size_t len);
int bloom_filter_add(bloom_filter_t *self, const char *key, size_t len);

void bloom_filter_print(bloom_filter_t *self);

void bloom_filter_destroy(bloom_filter_t *self);

bool bloom_filter_write(bloom_filter_t *self, FILE *f);
bool bloom_filter_save(bloom_filter_t *self, char *path);

bloom_filter_t *bloom_filter_read(FILE *f);
bloom_filter_t *bloom_filter_load(char *path);


#endif