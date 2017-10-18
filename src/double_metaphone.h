#ifndef DOUBLE_METAPHONE__H
#define DOUBLE_METAPHONE__H

#include <stdio.h>
#include <stdlib.h>

typedef struct double_metaphone_codes {
    char *primary;
    char *secondary;
} double_metaphone_codes_t;

double_metaphone_codes_t *double_metaphone(char *input);

void double_metaphone_codes_destroy(double_metaphone_codes_t *codes);

#endif

