#include "stochastic_gradient_descent.h"

bool stochastic_gradient_descent(matrix_t *theta, matrix_t *gradient, double gamma) {
    if (gradient->m != theta->m || gradient->n != theta->n) {
        return false;
    }

    size_t m = gradient->m;
    size_t n = gradient->n;

    return matrix_sub_matrix_times_scalar(theta, gradient, gamma);
}

bool stochastic_gradient_descent_sparse(matrix_t *theta, matrix_t *gradient, uint32_array *update_indices, double gamma) {
    if (gradient->m != theta->m || gradient->n != theta->n) {
        return false;
    }

    size_t m = gradient->m;
    size_t n = gradient->n;

    double *gradient_values = gradient->values;
    double *theta_values = theta->values;

    uint32_t *indices = update_indices->a;
    size_t num_updated = update_indices->n;

    for (size_t i = 0; i < num_updated; i++) {
        uint32_t row = indices[i];
        for (size_t j = 0; j < n; j++) {
            size_t idx = row * n + j;
            double value = gradient_values[idx];
            theta_values[idx] -= gamma * value;
        }
    }

    return true;
}


/*
Sparse regularization
---------------------

Stochastic/minibatch gradients can be decomposed into 2 updates
1. The derivative of the loss function itself (0 for features not observed in the current batch)
2. The derivative of the regularization term (applies to all weights)

Reference: http://leon.bottou.org/publications/pdf/tricks-2012.pdf

Here we take sparsity a step further and do "lazy" or "just-in-time" regularization.

Updating all the weights on each iteration requires m * n operations for each minibatch
regardless of the number of parameters active in the minibatch.

However, the "correct" value of a given parameter theta_ij is only really needed in two places:

1. Before computing the gradient, since the current value of theta is used in said computation
2. When we're done training the model and want to save/persist it

In L2 regularization, the derivative of the regularization term is simply:

lambda * theta

Since theta changes proportional to itself, we can rewrite this for multiple timesteps as:

theta_i *= e^(-lambda * t)

where t is the number of timesteps since theta_i was last updated. This requires storing
a vector of size n containing the last updated timestamps, as well the set of columns
used by the minibatch (this implementation assumes it is computed elsewehre and passed in).

In NLP applications, where the updates are very sparse, only a small fraction of the
features are likely to be active in a given batch.

This means that if, say, an infrequently used word like "fecund" or "bucolic" is seen
in only one or two batches in the entire training corpus, we only touch that parameter
twice (three times counting the finalization step), while still getting roughly the same
results as though we had done the per-iteration weight updates.
*/


inline double stochastic_gradient_descent_gamma_t(double gamma_0, double lambda, uint32_t t) {
    return gamma_0 / (1.0 + lambda * gamma_0 * (double)t);
}

static inline void regularize_row(double *theta_i, size_t n, double lambda, uint32_t last_updated, uint32_t t, double gamma) {
    uint32_t timesteps = t - last_updated;
    double update = exp(gamma * -lambda * timesteps);
    double_array_mul(theta_i, update, n);
}

bool stochastic_gradient_descent_regularize_weights(matrix_t *theta, uint32_array *update_indices, uint32_array *last_updated, uint32_t t, double lambda, double gamma_0) {
    if (lambda > 0.0) {        
        uint32_t *updates = last_updated->a;

        size_t n = theta->n;

        size_t batch_rows = update_indices->n;
        uint32_t *rows = update_indices->a;

        for (size_t i = 0; i < batch_rows; i++) {
            uint32_t row = rows[i];
            double *theta_i = matrix_get_row(theta, row);
            uint32_t last_updated = updates[row];

            double gamma_t  = stochastic_gradient_descent_gamma_t(gamma_0, lambda, t - last_updated);
            regularize_row(theta_i, n, lambda, last_updated, t, gamma_t);
            updates[row] = t;
        }

    }

    return true;
}

inline bool stochastic_gradient_descent_finalize_weights(matrix_t *theta, uint32_array *last_updated, uint32_t t, double lambda, double gamma_0) {
    if (lambda > 0.0) {
        uint32_t *updates = last_updated->a;
        size_t m = theta->m;
        size_t n = theta->n;

        for (size_t i = 0; i < m; i++) {
            double *theta_i = matrix_get_row(theta, i);
            uint32_t last_updated = updates[i];

            double gamma_t  = stochastic_gradient_descent_gamma_t(gamma_0, lambda, t - last_updated);
            regularize_row(theta_i, n, lambda, last_updated, t, gamma_t);
            updates[i] = t;
        }
    }
    return true;
}
