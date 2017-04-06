#ifndef CARTESIAN_PRODUCT_H
#define CARTESIAN_PRODUCT_H

#include <stdlib.h>
#include <stdarg.h>

#include "collections.h"

typedef struct cartesian_product_iterator {
    size_t remaining;
    uint32_array *lengths;
    uint32_array *state;
} cartesian_product_iterator_t;

cartesian_product_iterator_t *cartesian_product_iterator_new(size_t n, ...);
cartesian_product_iterator_t *cartesian_product_iterator_new_vargs(size_t n, va_list args);
uint32_t *cartesian_product_iterator_start(cartesian_product_iterator_t *self);
uint32_t *cartesian_product_iterator_next(cartesian_product_iterator_t *self);
bool cartesian_product_iterator_done(cartesian_product_iterator_t *self);

void cartesian_product_iterator_destroy(cartesian_product_iterator_t *self);

#endif