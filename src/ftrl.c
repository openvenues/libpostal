#include "ftrl.h"
#include "log/log.h"

// Follow-the-regularized leader (FTRL) Proximal

ftrl_trainer_t *ftrl_trainer_new(size_t m, size_t n, bool fit_intercept, double alpha, double beta, double lambda1, double lambda2) {
    ftrl_trainer_t *trainer = malloc(sizeof(ftrl_trainer_t));
    if (trainer == NULL) return NULL;

    trainer->z = double_matrix_new_zeros(m, n);
    if (trainer->z == NULL) {
        goto exit_ftrl_trainer_created;
    }

    trainer->fit_intercept = fit_intercept;
    trainer->alpha = alpha;
    trainer->beta = beta;
    trainer->lambda1 = lambda1;
    trainer->lambda2 = lambda2;

    trainer->num_features = m;

    trainer->learning_rates = double_array_new_zeros(m);
    if (trainer->learning_rates == NULL) {
        goto exit_ftrl_trainer_created;
    }

    return trainer;
exit_ftrl_trainer_created:
    ftrl_trainer_destroy(trainer);
    return NULL;
}

bool ftrl_trainer_reset_params(ftrl_trainer_t *self, double alpha, double beta, double lambda1, double lambda2) {
    double_matrix_zero(self->z);
    double_array_zero(self->learning_rates->a, self->learning_rates->n);

    self->alpha = alpha;
    self->beta = beta;
    self->lambda1 = lambda1;
    self->lambda2 = lambda2;
    return true;
}


bool ftrl_trainer_extend(ftrl_trainer_t *self, size_t m) {
    if (self == NULL || self->z == NULL || self->learning_rates == NULL) return false;

    if (!double_matrix_resize_fill_zeros(self->z, m, self->z->n) ||
        !double_array_resize_fill_zeros(self->learning_rates, m)) {
        return false;
    }

    self->num_features = m;

    return true;
}

bool ftrl_set_weights(ftrl_trainer_t *self, double_matrix_t *w, uint32_array *indices) {
    if (self == NULL || w == NULL) return false;

    size_t m = self->z->m;
    size_t n = self->z->n;

    size_t num_indices = m;

    if (indices != NULL) {
        num_indices = indices->n;
    }

    if (!double_matrix_resize(w, num_indices, n)) {
        log_error("Resizing weights failed\n");
        return false;
    }

    double lambda1 = self->lambda1;
    double lambda2 = self->lambda2;

    double_matrix_t *z = self->z;
    double *learning_rates = self->learning_rates->a;

    double alpha = self->alpha;
    double beta = self->beta;

    uint32_t *row_indices = NULL;

    if (indices != NULL) {
        row_indices = indices->a;
    }

    uint32_t row_idx;
    size_t i_start = self->fit_intercept ? 1 : 0;

    for (size_t i = 0; i < num_indices; i++) {
        if (indices != NULL) {
            row_idx = row_indices[i];            
        } else {
            row_idx = i;
        }

        double *row = double_matrix_get_row(z, (size_t)row_idx);
        double lr = learning_rates[row_idx];
        double *weights_row = double_matrix_get_row(w, i);

        if (row_idx >= i_start) {
            for (size_t j = 0; j < n; j++) {
                double z_ij = row[j];
                double sign_z_ij = sign(z_ij);
                if (sign_z_ij * z_ij > lambda1) {
                    double w_ij = -(1.0/(((beta + sqrt(lr)) / alpha) + lambda2)) * (z_ij - sign_z_ij * lambda1);
                    weights_row[j] = w_ij;
                } else {
                    weights_row[j] = 0.0;
                }
            }
        } else {
            for (size_t j = 0; j < n; j++) {
                double z_ij = row[j];
                double w_ij = -(1.0/((beta + sqrt(lr)) / alpha)) * z_ij;
                weights_row[j] = w_ij;
            }
        }
    }

    return true;
}

bool ftrl_update_gradient(ftrl_trainer_t *self, double_matrix_t *gradient, double_matrix_t *weights, uint32_array *indices, size_t batch_size) {
    if (self == NULL || indices == NULL || gradient == NULL || gradient->m != weights->m || gradient->n != weights->n) {
        if (indices == NULL) {
            log_error("indices was NULL\n");
        }
        log_error("gradient->m = %zu, gradient->n = %zu, weights->m = %zu, weights->n = %zu\n", gradient->m, gradient->n, weights->m, weights->n);
        return false;
    }

    size_t m = self->z->m;
    size_t n = self->z->n;

    size_t num_indices = indices->n;
    uint32_t *row_indices = indices->a;

    double_matrix_t *z = self->z;

    double *learning_rates = self->learning_rates->a;

    double alpha = self->alpha;

    for (size_t i = 0; i < num_indices; i++) {
        uint32_t row_idx = row_indices[i];
        if (row_idx >= m) {
            log_error("row_idx = %u, m = %zu\n", row_idx, m);
            return false;
        }
        double lr = learning_rates[row_idx];

        double *weights_row = double_matrix_get_row(weights, i);
        double *gradient_row = double_matrix_get_row(gradient, i);
        double *z_row = double_matrix_get_row(z, row_idx);

        double lr_update = lr;

        for (size_t j = 0; j < n; j++) {
            double grad_ij = gradient_row[j];
            lr_update += grad_ij * grad_ij;
        }

        double sigma = (1.0 / (alpha * batch_size)) * (sqrt(lr_update) - sqrt(lr));

        for (size_t j = 0; j < n; j++) {
            double z_ij_update = gradient_row[j] - sigma * weights_row[j];
            z_row[j] += z_ij_update;
        }

        learning_rates[row_idx] = lr_update;
    }

    return true;
}

double ftrl_reg_cost(ftrl_trainer_t *self, double_matrix_t *theta, uint32_array *update_indices, size_t batch_size) {
    double cost = 0.0;

    size_t m = theta->m;
    size_t n = theta->n;

    uint32_t *indices = NULL;
    size_t num_indices = m;

    if (update_indices != NULL) {
        uint32_t *indices = update_indices->a;
        size_t num_indices = update_indices->n;
    }
    size_t i_start = self->fit_intercept ? 1 : 0;

    double lambda1 = self->lambda1;
    double lambda2 = self->lambda2;

    double l2_cost = 0.0;
    double l1_cost = 0.0;

    for (size_t i = 0; i < m; i++) {
        uint32_t row_idx = i;
        if (indices != NULL) {
            row_idx = indices[i];
        }

        if (row_idx >= i_start) {
            double *theta_i = double_matrix_get_row(theta, i);

            l2_cost += double_array_sum_sq(theta_i, n);
            l1_cost += double_array_l1_norm(theta_i, n);
        }
    }

    cost += lambda2 / 2.0 * l2_cost;
    cost += lambda1 * l1_cost;

    return cost * 1.0 / (double)batch_size;
}


double_matrix_t *ftrl_weights_finalize(ftrl_trainer_t *self) {
    if (!ftrl_set_weights(self, self->z, NULL)) {
        return NULL;
    }

    double_matrix_t *weights = self->z;
    self->z = NULL;
    return weights;
}


sparse_matrix_t *ftrl_weights_finalize_sparse(ftrl_trainer_t *self) {
    size_t m = self->z->m;
    size_t n = self->z->n;

    double *learning_rates = self->learning_rates->a;

    double alpha = self->alpha;
    double beta = self->beta;
    double lambda1 = self->lambda1;
    double lambda2 = self->lambda2;

    sparse_matrix_t *weights = sparse_matrix_new();
    log_info("weights->m = %" PRIu32 "\n", weights->m);

    size_t i_start = 0;

    if (self->fit_intercept) {
        double *row = double_matrix_get_row(self->z, 0);
        double lr = learning_rates[0];

        for (size_t j = 0; j < n; j++) {
            double z_ij = row[j];
            double w_ij = -(1.0/((beta + sqrt(lr)) / alpha)) * z_ij;
            sparse_matrix_append(weights, j, w_ij);
        }
        sparse_matrix_finalize_row(weights);
        i_start = 1;
    }
    log_info("after intercept weights->m = %" PRIu32 "\n", weights->m);

    for (size_t i = i_start; i < m; i++) {
        double *row = double_matrix_get_row(self->z, (size_t)i);
        double lr = learning_rates[i];
        for (size_t j = 0; j < n; j++) {
            double z_ij = row[j];
            double sign_z_ij = sign(z_ij);
            if (sign_z_ij * z_ij > lambda1) {
                double w_ij = -(1.0/(((beta + sqrt(lr)) / alpha) + lambda2)) * (z_ij - sign_z_ij * lambda1);
                sparse_matrix_append(weights, j, w_ij);
            }
        }
        sparse_matrix_finalize_row(weights);

        if (i % 1000 == 0 && i > 0) {
            log_info("adding rows, weights->m = %" PRIu32 "\n", weights->m);
        }
    }

    return weights;
}


void ftrl_trainer_destroy(ftrl_trainer_t *self) {
    if (self == NULL) return;

    if (self->z != NULL) {
        double_matrix_destroy(self->z);
    }

    if (self->learning_rates != NULL) {
        double_array_destroy(self->learning_rates);
    }

    free(self);
}
