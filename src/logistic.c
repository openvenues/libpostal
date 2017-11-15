#include "logistic.h"
#include "vector_math.h"

inline double sigmoid(double x) {
    return 1.0/(1.0 + exp(-x));
}

inline void sigmoid_vector(double *x, size_t n) {
    for (int i = 0; i < n; i++) {
        x[i] = sigmoid(x[i]);
    }
}

inline void softmax_vector(double *x, size_t n) {
    int i;
    double sum = 0.0;
    
    double denom = double_array_log_sum_exp(x, n);

    for (i = 0; i < n; i++) {
        x[i] = exp(x[i] - denom);
    }
}


void softmax_matrix(double_matrix_t *matrix) {
    size_t num_rows = matrix->m;
    size_t num_cols = matrix->n;

    for (int i = 0; i < num_rows; i++) {
        double *values = double_matrix_get_row(matrix, i);
        softmax_vector(values, num_cols);
    }
}