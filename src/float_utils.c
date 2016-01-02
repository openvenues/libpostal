#include "float_utils.h"

inline bool float_equals_epsilon(float a, float b, float epsilon) {
    float diff = fabsf(a - b);
    a = fabsf(a);
    b = fabsf(b);

    float largest = (b > a) ? b : a;

    return (diff <= largest * epsilon);
}

inline bool float_equals(float a, float b) {
    return float_equals_epsilon(a, b, FLT_EPSILON);
}

inline bool double_equals_epsilon(double a, double b, double epsilon) {
    double diff = fabs(a - b);
    a = fabs(a);
    b = fabs(b);

    double largest = (b > a) ? b : a;

    return (diff <= largest * epsilon);
}

inline bool double_equals(double a, double b) {
    return double_equals_epsilon(a, b, DBL_EPSILON);
}
