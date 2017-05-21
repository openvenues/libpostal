#include <math.h>

#include "logistic_regression.h"
#include "logistic.h"
#include "file_utils.h"

bool logistic_regression_model_expectation_sparse(sparse_matrix_t *theta, sparse_matrix_t *x, double_matrix_t *p_y) {
    if (theta == NULL || x == NULL || p_y == NULL) {
        if (theta == NULL) log_error("theta = NULL\n");
        if (x == NULL) log_error("x = NULL\n");
        if (p_y == NULL) log_error("p_y = NULL\n");
        return false;
    }

    if (sparse_matrix_dot_sparse(x, theta, p_y) != 0) {
        log_error("x->m = %" PRIu32 ", x->n = %" PRIu32 ", theta->m = %" PRIu32 ", theta->n = %" PRIu32 ", p_y->m = %zu, p_y->n = %zu\n", x->m, x->n, theta->m, theta->n, p_y->m, p_y->n);
        return false;
    }

    softmax_matrix(p_y);

    return true;
}

bool logistic_regression_model_expectation(double_matrix_t *theta, sparse_matrix_t *x, double_matrix_t *p_y) {
    if (theta == NULL || x == NULL || p_y == NULL) {
        if (theta == NULL) log_error("theta = NULL\n");
        if (x == NULL) log_error("x = NULL\n");
        if (p_y == NULL) log_error("p_y = NULL\n");
        return false;
    }

    if (sparse_matrix_dot_dense(x, theta, p_y) != 0) {
        log_error("x->m = %" PRIu32 ", x->n = %" PRIu32 ", theta->m = %zu, theta->n = %zu, p_y->m = %zu, p_y->n = %zu\n", x->m, x->n, theta->m, theta->n, p_y->m, p_y->n);
        return false;
    }

    softmax_matrix(p_y);

    return true;
}

double logistic_regression_cost_function(double_matrix_t *theta, sparse_matrix_t *x, uint32_array *y, double_matrix_t *p_y) {
    size_t m = x->m;
    size_t n = x->n;
    if (m != y->n) {
        log_error("m = %zu, y->n = %zu\n", m, y->n);
        return -1.0;
    }

    if (!double_matrix_resize_aligned(p_y, x->m, theta->n, 16)) {
        log_error("resize_aligned failed\n");
        return -1.0;
    }

    double_matrix_zero(p_y);

    if (!logistic_regression_model_expectation(theta, x, p_y)) {
        log_error("model expectation failed\n");
        return -1.0;
    }

    double *expected_values = p_y->values;
    double cost = 0.0;

    for (size_t i = 0; i < p_y->m; i++) {
        uint32_t y_i = y->a[i];
        double value = double_matrix_get(p_y, i, y_i);
        cost += log(value);
    }

    cost *= -(1.0 / m);

    return cost;
}

bool logistic_regression_gradient(double_matrix_t *theta, double_matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, double_matrix_t *p_y) {
    size_t m = x->m;
    size_t n = x->n;
    if (m != y->n || theta->m != gradient->m || theta->n != gradient->n) return false;

    if (!double_matrix_resize_aligned(p_y, x->m, theta->n, 16)) {
        return false;
    }
    double_matrix_zero(p_y);

    if (!logistic_regression_model_expectation(theta, x, p_y)) {
        return false;
    }

    size_t num_features = n;
    size_t num_classes = p_y->n;
    uint32_t i, j;

    double residual;
  
    uint32_t row;
    uint32_t col;
    double data;

    double_matrix_zero(gradient);

    double *theta_values = theta->values;
    double *predicted_values = p_y->values;
    double *gradient_values = gradient->values;

    // gradient = -(1. / m) * x.T.dot(y - p_y)

    sparse_matrix_foreach(x, row, col, data, {
        uint32_t y_i = y->a[row];
        for (j = 0; j < num_classes; j++) {
            double class_prob = double_matrix_get(p_y, row, j);
            double residual = (y_i == j ? 1.0 : 0.0) - class_prob;
            double gradient_update = data * residual;
            double_matrix_add_scalar(gradient, col, j, gradient_update);
        }
    })

    double scale = -1.0 / m;
    double_matrix_mul(gradient, scale);

    return true;
}
