#ifndef FLOAT_UTILS_H
#define FLOAT_UTILS_H

#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>

bool float_equals(float a, float b);
bool float_equals_epsilon(float a, float b, float epsilon);
float fsign(float x);

bool double_equals(double a, double b);
bool double_equals_epsilon(double a, double b, double epsilon);
double sign(double x);

#endif

