#include <math.h>

#include "logistic_regression.h"
#include "logistic.h"
#include "file_utils.h"

#define NEAR_ZERO_WEIGHT 1e-6

bool logistic_regression_model_expectation(matrix_t *theta, sparse_matrix_t *x, matrix_t *p_y) {
    if (theta == NULL || x == NULL || p_y == NULL) return false;

    if (sparse_matrix_dot_dense(x, theta, p_y) != 0) {
        return false;
    }

    softmax_matrix(p_y);

    return true;
}


double logistic_regression_cost_function(matrix_t *theta, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, double lambda) {
    size_t m = x->m;
    size_t n = x->n;
    if (m != y->n) return -1.0;

    if (!matrix_resize(p_y, x->m, theta->n)) {
        return -1.0;
    }

    if (!logistic_regression_model_expectation(theta, x, p_y)) {
        return -1.0;
    }

    double *expected_values = p_y->values;
    double cost = 0.0;

    // - \frac{1}{m} \left[ \sum_{i=1}^{m} \sum_{j=1}^{k}  1\left\{y^{(i)} = j\right\} \log \frac{e^{\theta_j^T x^{(i)}}}{\sum_{l=1}^k e^{ \theta_l^T x^{(i)} }}\right]

    for (size_t i = 0; i < p_y->m; i++) {
        uint32_t y_i = y->a[i];
        double value = matrix_get(p_y, i, y_i);
        if (value > NEAR_ZERO_WEIGHT) {
            cost += log(value);
        }
    }

    cost *= -(1.0 / m);

    if (lambda > 0.0) {
        double reg_cost = 0.0;
        for (size_t i = 1; i < theta->m; i++) {
            for (size_t j = 0; j < theta->n; j++) {
                double theta_ij = matrix_get(theta, i, j);
                reg_cost += theta_ij * theta_ij;
            }

        }

        cost += reg_cost * (lambda / 2.0);
    }

    return cost;

}

bool logistic_regression_gradient(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, double lambda) {
    size_t m = x->m;
    size_t n = x->n;
    if (m != y->n) return false;

    if (!matrix_resize(p_y, x->m, theta->n) || !matrix_resize(p_y, x->m, theta->n)) {
        return false;
    }

    matrix_zero(gradient);

    if (!logistic_regression_model_expectation(theta, x, p_y)) {
        return false;
    }

    size_t num_classes = p_y->n;

    double residual;
  
    uint32_t row;
    uint32_t col;
    double data;

    uint32_t i, j;

    bool regularize = lambda > 0.0;

    // gradient = -(1. / m) * x.T.dot(y - p_y) + lambda * theta

    sparse_matrix_foreach(x, row, col, data, {
        uint32_t y_i = y->a[row];
        for (j = 0; j < num_classes; j++) {
            residual = (y_i == j ? 1.0 : 0.0) - matrix_get(p_y, row, j);
            double value = data * residual;
            matrix_add_scalar(gradient, col, j, value);
        }
    })

    matrix_mul(gradient, -1.0 / m);

    if (regularize) {
        for (i = 1; i < m; i++) {
            for (j = 0; j < n; j++) {
                double theta_ij = matrix_get(theta, i, j);
                double reg_value = theta_ij * lambda;
                matrix_add_scalar(gradient, i, j, reg_value);
            }
        }

    }

    return true;
}
