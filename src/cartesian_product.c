#include "cartesian_product.h"
#include <stdarg.h>

cartesian_product_iterator_t *cartesian_product_iterator_new_vargs(size_t n, va_list args) {
    cartesian_product_iterator_t *iter = malloc(sizeof(cartesian_product_iterator_t));
    if (iter == NULL) return NULL;

    iter->lengths = uint32_array_new_size(n);
    if (iter->lengths == NULL) {
        goto exit_iter_created;
    }

    size_t remaining = 1;
    for (size_t i = 0; i < n; i++) {
        uint32_t arg = va_arg(args, uint32_t);
        uint32_array_push(iter->lengths, arg);
        if (arg > 0) {
            remaining *= arg;
        }
    }
    iter->remaining = remaining;

    iter->state = uint32_array_new_zeros(n);
    if (iter->state == NULL) {
        goto exit_iter_created;
    }

    return iter;

exit_iter_created:
    cartesian_product_iterator_destroy(iter);
    return NULL;
}

cartesian_product_iterator_t *cartesian_product_iterator_new(size_t n, ...) {
    va_list args;
    va_start(args, n);
    cartesian_product_iterator_t *iter = cartesian_product_iterator_new_vargs(n, args);
    va_end(args);
    return iter;
}

uint32_t *cartesian_product_iterator_start(cartesian_product_iterator_t *self) {
    return self->state->a;
}

bool cartesian_product_iterator_done(cartesian_product_iterator_t *self) {
    return self->remaining == 0;
}

uint32_t *cartesian_product_iterator_next(cartesian_product_iterator_t *self) {
    if (self == NULL) return NULL;

    uint32_t *lengths = self->lengths->a;
    uint32_t *state = self->state->a;

    if (self->remaining > 0) {
        ssize_t i;
        ssize_t n = self->lengths->n;
        for (i = n - 1; i >= 0; i--) {
            state[i]++;
            if (state[i] == lengths[i]) {
                state[i] = 0;
            } else {
                self->remaining--;
                break;
            }
        }
        if (i < 0) {
            self->remaining = 0;
        }
    } else {
        return NULL;
    }

    return state;
}

void cartesian_product_iterator_destroy(cartesian_product_iterator_t *self) {
    if (self == NULL) return;

    if (self->lengths != NULL) {
        uint32_array_destroy(self->lengths);
    }

    if (self->state != NULL) {
        uint32_array_destroy(self->state);
    }

    free(self);
}
