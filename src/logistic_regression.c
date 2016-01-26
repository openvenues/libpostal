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

static bool logistic_regression_gradient_params(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, 
                                                uint32_array *x_cols, double lambda) {
    size_t m = x->m;
    size_t n = x->n;
    if (m != y->n) return false;

    if (!matrix_resize(p_y, x->m, theta->n) || !matrix_resize(p_y, x->m, theta->n)) {
        return false;
    }

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


    bool regularize = lambda > 0.0;

    double *theta_values = theta->values;
    double *predicted_values = p_y->values;
    double *gradient_values = gradient->values;

    // Zero the relevant rows of the gradient
    if (x_cols != NULL) {
        double *gradient_i;

        size_t batch_rows = x_cols->n;
        uint32_t *cols = x_cols->a;
        for (i = 0; i < batch_rows; i++) {
            col = cols[i];
            gradient_i = matrix_get_row(gradient, col);
            double_array_zero(gradient_i, num_classes);
        }
    } else {
        matrix_zero(gradient);
    }

    // gradient = -(1. / m) * x.T.dot(y - p_y) + lambda * theta

    sparse_matrix_foreach(x, row, col, data, {
        uint32_t y_i = y->a[row];
        for (j = 0; j < num_classes; j++) {

            double class_prob = predicted_values[row * num_classes + j];
            double residual = (y_i == j ? 1.0 : 0.0) - class_prob;
            gradient_values[col * num_classes + j] += data * residual;
        }
    })

    double scale = -1.0 / m;

    // Scale the vector by -1 / m using only the unique columns in X 
    // Useful for stochastic and minibatch gradients
    if (x_cols != NULL) {
        size_t batch_rows = x_cols->n;
        uint32_t *cols = x_cols->a;
        for (i = 0; i < batch_rows; i++) {
            col = cols[i];
            for (j = 0; j < num_classes; j++) {
                gradient_values[col * num_classes + j] *= scale;
            }
        }
    } else {
        matrix_mul(gradient, scale);
    }


    // Update the only the relevant columns in x
    if (regularize) {
        size_t num_rows = num_features;
        uint32_t *cols = NULL;

        if (x_cols != NULL) {
            cols = x_cols->a;
            num_rows = x_cols->n;
        }

        for (i = 0; i < num_rows; i++) {
            col = x_cols != NULL ? cols[i] : i;

            for (j = 0; j < num_classes; j++) {
                size_t idx = col * num_classes + j;
                double theta_ij = theta_values[idx];
                double reg_update = theta_ij * lambda;
                double current_value = gradient_values[idx];
                double updated_value = current_value + reg_update;
                if ((updated_value > 0) == (current_value > 0)) {
                    gradient_values[idx] = updated_value;
                }
            }
        }
    }

    return true;
}

inline bool logistic_regression_gradient_sparse(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, 
                                                uint32_array *x_cols, double lambda) {
    return logistic_regression_gradient_params(theta, gradient, x, y, p_y, x_cols, lambda);
}


inline bool logistic_regression_gradient(matrix_t *theta, matrix_t *gradient, sparse_matrix_t *x, uint32_array *y, matrix_t *p_y, double lambda) {
    return logistic_regression_gradient_params(theta, gradient, x, y, p_y, NULL, lambda);
}
