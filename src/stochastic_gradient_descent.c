#include "stochastic_gradient_descent.h"

bool stochastic_gradient_descent(matrix_t *theta, matrix_t *gradient, double gamma) {
    if (gradient->m != theta->m || gradient->n != theta->n) {
        return false;
    }

    size_t m = gradient->m;
    size_t n = gradient->n;

    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            double grad_ij = matrix_get(gradient, i, j);
            matrix_sub_scalar(theta, i, j, gamma * grad_ij);
        }
    }

    return true;
}

inline bool stochastic_gradient_descent_scheduled(matrix_t *theta, matrix_t *gradient, float lambda, uint32_t t, double gamma_0) {
    double gamma = gamma_0 / (1.0 + lambda * gamma_0 * (double)t);

    return stochastic_gradient_descent(theta, gradient, gamma);
}
