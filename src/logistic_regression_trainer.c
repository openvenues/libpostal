#include "logistic_regression_trainer.h"

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

    free(self);
}

logistic_regression_trainer_t *logistic_regression_trainer_init(trie_t *feature_ids, khash_t(str_uint32) *label_ids) {
    if (feature_ids == NULL || label_ids == NULL) return NULL;

    logistic_regression_trainer_t *trainer = malloc(sizeof(logistic_regression_trainer_t));
    if (trainer == NULL) return NULL;

    trainer->feature_ids = feature_ids;
    // Add one feature for the bias unit
    trainer->num_features = trie_num_keys(feature_ids) + 1;

    trainer->label_ids = label_ids;
    trainer->num_labels = kh_size(label_ids);

    trainer->weights = matrix_new_zeros(trainer->num_features, trainer->num_labels);

    trainer->lambda = DEFAULT_LAMBDA;
    trainer->iters = 0;
    trainer->epochs = 0;
    trainer->gamma_0 = DEFAULT_GAMMA_0;
    trainer->gamma = DEFAULT_GAMMA;

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

    matrix_t *gradient = matrix_new_zeros(m, n);

    sparse_matrix_t *x = feature_matrix(self->feature_ids, features);
    uint32_array *y = label_vector(self->label_ids, labels);

    matrix_t *p_y = matrix_new_zeros(x->m, n);

    bool ret = false;

    if (!logistic_regression_gradient(self->weights, gradient, x, y, p_y, self->lambda)) {
        log_error("Gradient failed\n");
        goto exit_matrices_created;
    }

    size_t data_len = m * n;

    ret = stochastic_gradient_descent(self->weights, gradient, self->gamma);

    self->iters++;

exit_matrices_created:
    matrix_destroy(gradient);
    matrix_destroy(p_y);
    uint32_array_destroy(y);
    sparse_matrix_destroy(x);
    return ret;
}
