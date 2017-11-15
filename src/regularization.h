#ifndef REGULARIZATION_H
#define REGULARIZATION_H

#include <stdlib.h>

typedef enum {
    REGULARIZATION_NONE,
    REGULARIZATION_L1,
    REGULARIZATION_L2
} regularization_type_t;

void regularize_l2(double *theta, size_t n, double reg_update);
void regularize_l1(double *theta, size_t n, double reg_update);

#endif