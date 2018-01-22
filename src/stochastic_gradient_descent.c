#include "stochastic_gradient_descent.h"
#include "sparse_matrix_utils.h"

sgd_trainer_t *sgd_trainer_new(size_t m, size_t n, bool fit_intercept, regularization_type_t reg_type, double lambda, double gamma_0) {
    sgd_trainer_t *sgd = calloc(1, sizeof(sgd_trainer_t));
    if (sgd == NULL) return NULL;

    double_matrix_t *theta = double_matrix_new_zeros(m, n);
    if (theta == NULL) {
        log_error("Error allocating weights\n");
        goto exit_sgd_trainer_created;
    }

    sgd->fit_intercept = fit_intercept;
    sgd->theta = theta;

    sgd->reg_type = reg_type;

    sgd->lambda = lambda;

    if (reg_type != REGULARIZATION_NONE) {
        sgd->last_updated = uint32_array_new_zeros(m);
        if (sgd->last_updated == NULL) {
            goto exit_sgd_trainer_created;
        }

        sgd->penalties = double_array_new();
        if (sgd->penalties == NULL) {
            goto exit_sgd_trainer_created;
        }
        // Penalty for last_updated == 0 is 0
        double_array_push(sgd->penalties, 0.0);
    } else {
        sgd->last_updated = NULL;
        sgd->penalties = NULL;
    }

    sgd->gamma_0 = gamma_0;
    sgd->iterations = 0;

    return sgd;

exit_sgd_trainer_created:
    sgd_trainer_destroy(sgd);
    return NULL;
}


bool sgd_trainer_reset_params(sgd_trainer_t *self, double lambda, double gamma_0) {
    regularization_type_t reg_type = self->reg_type;
    if (reg_type != REGULARIZATION_NONE) {
        if (self->last_updated == NULL) {
            self->last_updated = uint32_array_new_zeros(self->theta->m);
            if (self->last_updated == NULL) return false;
        } else {
            uint32_array_zero(self->last_updated->a, self->last_updated->n);
        }

        if (self->penalties == NULL) {
            self->penalties = double_array_new();
            if (self->penalties == NULL) return false;
        } else {
            double_array_clear(self->penalties);
        }
        double_array_push(self->penalties, 0.0);
    }

    double_matrix_zero(self->theta);
    self->iterations = 0;
    self->lambda = lambda;
    self->gamma_0 = gamma_0;
    return true;
}


static inline double stochastic_gradient_descent_gamma_t(double gamma_0, double lambda, uint32_t t) {
    return gamma_0 / (1.0 + lambda * gamma_0 * (double)t);
}

static inline void gradient_update_row(double *theta_i, double *grad_i, size_t n, double gamma_t) {
    for (size_t j = 0; j < n; j++) {
        theta_i[j] -= gamma_t * grad_i[j];
    }
}

bool stochastic_gradient_descent_update(sgd_trainer_t *self, double_matrix_t *gradient, size_t batch_size) {
    if (self == NULL || self->theta == NULL || gradient == NULL ||
        gradient->m != self->theta->m || gradient->n != self->theta->n) {
        return false;
    }

    size_t m = gradient->m;
    size_t n = gradient->n;

    double lambda = self->lambda;
    double gamma_t = stochastic_gradient_descent_gamma_t(self->gamma_0, lambda, self->iterations);

    double_matrix_t *theta = self->theta;

    size_t i_start = self->fit_intercept ? 1 : 0;

    regularization_type_t reg_type = self->reg_type;

    double lambda_update = 0.0;
    if (reg_type != REGULARIZATION_NONE) {
        lambda_update = lambda / (double)batch_size * gamma_t;
    }

    for (size_t i = 0; i < m; i++) {
        double *theta_i = double_matrix_get_row(theta, i);
        double *grad_i = double_matrix_get_row(gradient, i);

        gradient_update_row(theta_i, grad_i, n, gamma_t);

        if (reg_type == REGULARIZATION_L2 && i >= i_start) {
            regularize_l2(theta_i, n, lambda_update);
        } else if (reg_type == REGULARIZATION_L1 && i >= i_start) {
            regularize_l1(theta_i, n, lambda_update);
        }
    }

    self->iterations++;

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

bool stochastic_gradient_descent_update_sparse(sgd_trainer_t *self, double_matrix_t *gradient, uint32_array *update_indices, size_t batch_size) {
    if (self == NULL) {
        log_info("self = NULL\n");
        return false;
    }
    double_matrix_t *theta = self->theta;

    if (gradient->n != theta->n) {
        log_info("gradient->n = %zu, theta->n = %zu\n", gradient->n, theta->n);
        return false;
    }

    size_t n = self->theta->n;

    uint32_t t = self->iterations;

    uint32_t *indices = update_indices->a;
    size_t num_updated = update_indices->n;

    uint32_t *updates = self->last_updated->a;

    size_t i_start = self->fit_intercept ? 1 : 0;

    double lambda = self->lambda;
    double gamma_0 = self->gamma_0;
    double gamma_t = stochastic_gradient_descent_gamma_t(gamma_0, lambda, t);

    regularization_type_t reg_type = self->reg_type;

    double lambda_update = 0.0;
    double penalty = 0.0;

    double *penalties = self->penalties->a;

    if (reg_type != REGULARIZATION_NONE) {
        lambda_update = lambda / (double)batch_size * gamma_t;

        if (t > self->penalties->n) {
            log_info("t = %" PRIu32 ", penalties->n = %zu\n", t, self->penalties->n);
            return false;
        }
        penalty = self->penalties->a[t];
    }

    for (size_t i = 0; i < num_updated; i++) {
        uint32_t col = indices[i];
        double *theta_i = double_matrix_get_row(theta, col);
        double *grad_i = double_matrix_get_row(gradient, i);

        uint32_t last_updated = updates[col];
        double last_update_penalty = 0.0;

        if (self->iterations > 0) {
            if (last_updated >= self->penalties->n) {
                log_info("col = %u, t = %" PRIu32 ", last_updated = %" PRIu32 ", penalties->n = %zu\n", col, t, last_updated, self->penalties->n);
                return false;
            }

            last_update_penalty = penalties[last_updated];

            // Update the weights to what they would have been
            // if all the regularization updates were applied

            if (last_updated < t) {
                double penalty_update = penalty - last_update_penalty;

                if (reg_type == REGULARIZATION_L2 && col >= i_start) {
                    regularize_l2(theta_i, n, penalty_update);
                } else if (reg_type == REGULARIZATION_L1 && col >= i_start) {
                    regularize_l1(theta_i, n, penalty_update);
                }
            }
        }

        // Update the gradient for the observed features in this batch

        gradient_update_row(theta_i, grad_i, n, gamma_t);

        // Add the regularization update for this iteration
        // so the weights are correct for the next gradient computation

        if (reg_type == REGULARIZATION_L2 && col >= i_start) {
            regularize_l2(theta_i, n, lambda_update);
        } else if (reg_type == REGULARIZATION_L1 && col >= i_start) {
            regularize_l1(theta_i, n, lambda_update);
        }

        // Set the last updated timestep for this feature to time t + 1
        // since we're upating the iteration count
        updates[col] = t + 1;
    }

    if (reg_type != REGULARIZATION_NONE) {
        // Add the cumulative penalty at time t to the penalties array
        double_array_push(self->penalties, penalty + lambda_update);
    }

    self->iterations++;

    return true;
}

double stochastic_gradient_descent_reg_cost(sgd_trainer_t *self, uint32_array *update_indices, size_t batch_size) {
    double cost = 0.0;

    regularization_type_t reg_type = self->reg_type;

    if (reg_type == REGULARIZATION_NONE) return cost;

    double_matrix_t *theta = self->theta;
    size_t m = theta->m;
    size_t n = theta->n;

    uint32_t *indices = NULL;
    size_t num_indices = m;

    if (update_indices != NULL) {
        uint32_t *indices = update_indices->a;
        size_t num_indices = update_indices->n;
    }
    size_t i_start = self->fit_intercept ? 1 : 0;

    for (size_t i = 0; i < num_indices; i++) {
        uint32_t row = i;
        if (indices != NULL) {
            row = indices[i];
        }
        double *theta_i = double_matrix_get_row(theta, row);

        if (reg_type == REGULARIZATION_L2 && row >= i_start) {
            cost += double_array_sum_sq(theta_i, n);
        } else if (reg_type == REGULARIZATION_L1 && row >= i_start) {
            cost += double_array_l1_norm(theta_i, n);
        }
    }

    if (reg_type == REGULARIZATION_L2) {
        cost *= self->lambda / 2.0;
    } else if (reg_type == REGULARIZATION_L1) {
        cost *= self->lambda;
    }

    return cost / (double)batch_size;
}

bool stochastic_gradient_descent_set_regularized_weights(sgd_trainer_t *self, double_matrix_t *w, uint32_array *indices) {
    if (self == NULL || self->theta == NULL) {
        if (self->theta == NULL) {
            log_info("stochastic_gradient_descent_regularize_weights theta NULL\n");
        }
        return false;
    }

    double lambda = self->lambda;
    double gamma_0 = self->gamma_0;
    regularization_type_t reg_type = self->reg_type;

    double_matrix_t *theta = self->theta;

    size_t m = theta->m;
    size_t n = theta->n;

    uint32_t *row_indices = NULL;
    size_t num_indices = m;

    if (indices != NULL) {
        row_indices = indices->a;
        num_indices = indices->n;
    }

    uint32_t *updates = self->last_updated->a;
    double *penalties = self->penalties->a;

    if (w != NULL && !double_matrix_resize(w, num_indices, n)) {
        log_error("Resizing weights failed\n");
        return false;
    }

    size_t i_start = self->fit_intercept ? 1 : 0;
    bool regularize = lambda > 0.0 && reg_type != REGULARIZATION_NONE;

    for (size_t i = 0; i < num_indices; i++) {
        uint32_t row_idx = i;
        if (indices != NULL) {
            row_idx = row_indices[i];
        }

        double *theta_i = double_matrix_get_row(theta, row_idx);
        double *w_i = theta_i;
        if (w != NULL) {
            w_i = double_matrix_get_row(w, i);
            double_array_raw_copy(w_i, theta_i, n);
        }

        if (regularize && i >= i_start) {
            double most_recent_penalty = 0.0;
            uint32_t most_recent_iter = 0;

            if (self->iterations > 0) {
                most_recent_iter = self->iterations;
                if (most_recent_iter >= self->penalties->n) {
                    log_error("penalty_index (%u) >= self->penalties->n (%zu)\n", most_recent_iter, self->penalties->n);
                    return false;
                }
                most_recent_penalty = penalties[most_recent_iter];
            } else {
                most_recent_penalty = lambda / gamma_0;
            }

            uint32_t last_updated = updates[i];
            if (last_updated >= self->penalties->n) {
                log_error("last_updated (%" PRIu32 ") >= self->penalties-> (%zu)\n", last_updated, self->penalties->n);
                return false;
            }
            double last_update_penalty = penalties[last_updated];

            if (last_updated < most_recent_iter) {
                double penalty_update = most_recent_penalty - last_update_penalty;

                if (reg_type == REGULARIZATION_L2) {
                    regularize_l2(w_i, n, penalty_update);
                } else if (reg_type == REGULARIZATION_L1) {
                    regularize_l1(w_i, n, penalty_update);
                }
            }
        }

    }

    return true;
}


bool stochastic_gradient_descent_regularize_weights(sgd_trainer_t *self) {
    return stochastic_gradient_descent_set_regularized_weights(self, NULL, NULL);
}

double_matrix_t *stochastic_gradient_descent_get_weights(sgd_trainer_t *self) {
    if (!stochastic_gradient_descent_regularize_weights(self)) {
        log_info("stochastic_gradient_descent_regularize_weights returned false\n");
        return NULL;
    }

    return self->theta;
}

sparse_matrix_t *stochastic_gradient_descent_get_weights_sparse(sgd_trainer_t *self) {
    if (!stochastic_gradient_descent_regularize_weights(self)) {
        return NULL;
    }

    return sparse_matrix_new_from_matrix(self->theta);
}

void sgd_trainer_destroy(sgd_trainer_t *self) {
    if (self == NULL) return;

    if (self->theta != NULL) {
        double_matrix_destroy(self->theta);
    }

    if (self->last_updated != NULL) {
        uint32_array_destroy(self->last_updated);
    }

    if (self->penalties != NULL) {
        double_array_destroy(self->penalties);
    }

    free(self);
}
