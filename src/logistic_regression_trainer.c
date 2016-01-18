#include "logistic_regression_trainer.h"
#include "sparse_matrix_utils.h"

void logistic_regression_trainer_destroy(logistic_regression_trainer_t *self) {
    if (self == NULL) return;

    if (self->feature_ids != NULL) {
        trie_destroy(self->feature_ids);
    }

    if (self->label_ids != NULL) {
        kh_destroy(str_uint32, self->label_ids);
    }

    if (self->weights != NULL) {
        matrix_destroy(self->weights);
    }

    if (self->last_updated != NULL) {
        uint32_array_destroy(self->last_updated);
    }

    if (self->unique_columns != NULL) {
        kh_destroy(int_set, self->unique_columns);
    }
    
    if (self->batch_columns != NULL) {
        uint32_array_destroy(self->batch_columns);
    }

    if (self->gradient != NULL) {
        matrix_destroy(self->gradient);
    }

    free(self);
}

logistic_regression_trainer_t *logistic_regression_trainer_init(trie_t *feature_ids, khash_t(str_uint32) *label_ids, double gamma_0, double lambda) {
    if (feature_ids == NULL || label_ids == NULL) return NULL;

    logistic_regression_trainer_t *trainer = malloc(sizeof(logistic_regression_trainer_t));
    if (trainer == NULL) return NULL;

    trainer->feature_ids = feature_ids;
    // Add one feature for the bias unit
    trainer->num_features = trie_num_keys(feature_ids) + 1;

    trainer->label_ids = label_ids;
    trainer->num_labels = kh_size(label_ids);

    trainer->weights = matrix_new_zeros(trainer->num_features, trainer->num_labels);

    trainer->gradient = matrix_new_zeros(trainer->num_features, trainer->num_labels);

    trainer->unique_columns = kh_init(int_set);
    trainer->batch_columns = uint32_array_new_size(trainer->num_features);

    trainer->last_updated = uint32_array_new_zeros(trainer->num_features);

    trainer->lambda = lambda;
    trainer->iters = 0;
    trainer->epochs = 0;
    trainer->gamma_0 = gamma_0;

    return trainer;

exit_trainer_created:
    logistic_regression_trainer_destroy(trainer);
    return NULL;
}


static matrix_t *model_expectation(sparse_matrix_t *x, matrix_t *theta) {
    matrix_t *p_y = matrix_new_zeros(x->m, theta->n);
    if (p_y == NULL) return NULL;

    if(logistic_regression_model_expectation(theta, x, p_y)) {
        return p_y;
    } else {
        matrix_destroy(p_y);
        return NULL;
    }
}

double logistic_regression_trainer_batch_cost(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels) {
    size_t m = self->weights->m;
    size_t n = self->weights->n;

    sparse_matrix_t *x = feature_matrix(self->feature_ids, features);
    uint32_array *y = label_vector(self->label_ids, labels);
    matrix_t *p_y = matrix_new_zeros(x->m, n);

    double cost = logistic_regression_cost_function(self->weights, x, y, p_y, self->lambda);

    matrix_destroy(p_y);
    uint32_array_destroy(y);
    sparse_matrix_destroy(x);
    return cost;    
}

bool logistic_regression_trainer_train_batch(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels) {
    size_t m = self->weights->m;
    size_t n = self->weights->n;

    // Optimize
    matrix_t *gradient = self->gradient;

    sparse_matrix_t *x = feature_matrix(self->feature_ids, features);
    uint32_array *y = label_vector(self->label_ids, labels);

    matrix_t *p_y = matrix_new_zeros(x->m, n);

    bool ret = false;

    if (!sparse_matrix_add_unique_columns(x, self->unique_columns, self->batch_columns)) {
        log_error("Unique columns failed\n");
        goto exit_matrices_created;
    }

    if (self->lambda > 0.0 && !stochastic_gradient_descent_regularize_weights(self->weights, self->batch_columns, self->last_updated, self->iters, self->lambda, self->gamma_0)) {
        log_error("Error regularizing weights\n");
        goto exit_matrices_created;
    }

    if (!logistic_regression_gradient_sparse(self->weights, gradient, x, y, p_y, self->batch_columns, self->lambda)) {
        log_error("Gradient failed\n");
        goto exit_matrices_created;
    }

    size_t data_len = m * n;

    double gamma = stochastic_gradient_descent_gamma_t(self->gamma_0, self->lambda, self->iters);
    ret = stochastic_gradient_descent_sparse(self->weights, gradient, self->batch_columns, gamma);

    self->iters++;

exit_matrices_created:
    matrix_destroy(p_y);
    uint32_array_destroy(y);
    sparse_matrix_destroy(x);
    return ret;
}

bool logistic_regression_trainer_finalize(logistic_regression_trainer_t *self) {
    if (self == NULL) return false;

    if (self->lambda > 0.0) {
        return stochastic_gradient_descent_finalize_weights(self->weights, self->last_updated, self->iters, self->lambda, self->gamma_0);
    }

    return true;
}
