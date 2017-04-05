#include "logistic_regression_trainer.h"
#include "sparse_matrix_utils.h"

#define INITIAL_FEATURE_BATCH_SIZE 1024

void logistic_regression_trainer_destroy(logistic_regression_trainer_t *self) {
    if (self == NULL) return;

    if (self->feature_ids != NULL) {
        trie_destroy(self->feature_ids);
    }

    if (self->label_ids != NULL) {
        kh_destroy(str_uint32, self->label_ids);
    }

    if (self->unique_columns != NULL) {
        kh_destroy(int_uint32, self->unique_columns);
    }
    
    if (self->batch_columns != NULL) {
        uint32_array_destroy(self->batch_columns);
    }

    if (self->batch_weights != NULL) {
        double_matrix_destroy(self->batch_weights);
    }

    if (self->gradient != NULL) {
        double_matrix_destroy(self->gradient);
    }

    free(self);
}

static logistic_regression_trainer_t *logistic_regression_trainer_init(trie_t *feature_ids, khash_t(str_uint32) *label_ids) {
    if (feature_ids == NULL || label_ids == NULL) return NULL;

    logistic_regression_trainer_t *trainer = malloc(sizeof(logistic_regression_trainer_t));
    if (trainer == NULL) return NULL;

    trainer->feature_ids = feature_ids;
    // Add one feature for the bias unit
    trainer->num_features = trie_num_keys(feature_ids) + 1;

    trainer->label_ids = label_ids;
    trainer->num_labels = kh_size(label_ids);

    trainer->gradient = double_matrix_new_zeros(INITIAL_FEATURE_BATCH_SIZE, trainer->num_labels);
    if (trainer->gradient == NULL) {
        goto exit_trainer_created;
    }

    trainer->unique_columns = kh_init(int_uint32);
    if (trainer->unique_columns == NULL) {
        goto exit_trainer_created;
    }
    trainer->batch_columns = uint32_array_new_size(INITIAL_FEATURE_BATCH_SIZE);
    if (trainer->batch_columns == NULL) {
        goto exit_trainer_created;
    }

    trainer->batch_weights = double_matrix_new_zeros(INITIAL_FEATURE_BATCH_SIZE, trainer->num_labels);
    if (trainer->batch_weights == NULL) {
        goto exit_trainer_created;
    }

    trainer->epochs = 0;

    return trainer;

exit_trainer_created:
    logistic_regression_trainer_destroy(trainer);
    return NULL;
}

logistic_regression_trainer_t *logistic_regression_trainer_init_sgd(trie_t *feature_ids, khash_t(str_uint32) *label_ids, bool fit_intercept, regularization_type_t reg_type, double lambda, double gamma_0) {
    logistic_regression_trainer_t *trainer = logistic_regression_trainer_init(feature_ids, label_ids);
    if (trainer == NULL) {
        return NULL;
    }

    trainer->optimizer_type = LOGISTIC_REGRESSION_OPTIMIZER_SGD;
    trainer->optimizer.sgd = sgd_trainer_new(trainer->num_features, trainer->num_labels, fit_intercept, reg_type, lambda, gamma_0);
    if (trainer->optimizer.sgd == NULL) {
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    return trainer;
}

logistic_regression_trainer_t *logistic_regression_trainer_init_ftrl(trie_t *feature_ids, khash_t(str_uint32) *label_ids, double lambda1, double lambda2, double alpha, double beta) {
    logistic_regression_trainer_t *trainer = logistic_regression_trainer_init(feature_ids, label_ids);
    if (trainer == NULL) {
        return NULL;
    }

    trainer->optimizer_type = LOGISTIC_REGRESSION_OPTIMIZER_FTRL;
    bool fit_intercept = true;
    log_info("num_features = %zu\n", trainer->num_features);
    trainer->optimizer.ftrl = ftrl_trainer_new(trainer->num_features, trainer->num_labels, fit_intercept, alpha, beta, lambda1, lambda2);
    if (trainer->optimizer.sgd == NULL) {
        logistic_regression_trainer_destroy(trainer);
        return NULL;
    }

    return trainer;
}

bool logistic_regression_trainer_reset_params_sgd(logistic_regression_trainer_t *self, double lambda, double gamma_0) {
    if (self == NULL || self->optimizer_type != LOGISTIC_REGRESSION_OPTIMIZER_SGD || self->optimizer.sgd == NULL) return false;

    sgd_trainer_t *sgd_trainer = self->optimizer.sgd;
    return sgd_trainer_reset_params(sgd_trainer, lambda, gamma_0);
}

bool logistic_regression_trainer_reset_params_ftrl(logistic_regression_trainer_t *self, double alpha, double beta, double lambda1, double lambda2) {
    if (self == NULL || self->optimizer_type != LOGISTIC_REGRESSION_OPTIMIZER_FTRL || self->optimizer.ftrl == NULL) return false;

    ftrl_trainer_t *ftrl_trainer = self->optimizer.ftrl;
    return ftrl_trainer_reset_params(ftrl_trainer, alpha, beta, lambda1, lambda2);
}

static double logistic_regression_trainer_minibatch_cost_params(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels, bool regularized) {
    size_t n = self->num_labels;

    sparse_matrix_t *x = feature_matrix(self->feature_ids, features);
    uint32_array *y = label_vector(self->label_ids, labels);
    double_matrix_t *p_y = double_matrix_new_aligned(x->m, n, 16);
    double_matrix_zero(p_y);

    double cost;

    if (!sparse_matrix_add_unique_columns_alias(x, self->unique_columns, self->batch_columns)) {
        cost = -1.0;
        goto exit_cost_matrices_created;
    }

    double_matrix_t *weights = logistic_regression_trainer_get_weights(self);

    cost = logistic_regression_cost_function(weights, x, y, p_y);

    if (regularized) {
        if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
            sgd_trainer_t *sgd_trainer = self->optimizer.sgd;
            double reg_cost = stochastic_gradient_descent_reg_cost(sgd_trainer, self->batch_columns, x->m);
            cost += reg_cost;
        } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
            ftrl_trainer_t *ftrl_trainer = self->optimizer.ftrl;
            double reg_cost = ftrl_reg_cost(ftrl_trainer, weights, self->batch_columns, x->m);
            cost += reg_cost;
        }
    }

exit_cost_matrices_created:
    double_matrix_destroy(p_y);
    uint32_array_destroy(y);
    sparse_matrix_destroy(x);
    return cost;
}

inline double logistic_regression_trainer_minibatch_cost(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels) {
    return logistic_regression_trainer_minibatch_cost_params(self, features, labels, false);
}

inline double logistic_regression_trainer_minibatch_cost_regularized(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels) {
    return logistic_regression_trainer_minibatch_cost_params(self, features, labels, true);
}

double logistic_regression_trainer_regularization_cost(logistic_regression_trainer_t *self, size_t m) {
    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        sgd_trainer_t *sgd_trainer = self->optimizer.sgd;
        return stochastic_gradient_descent_reg_cost(sgd_trainer, NULL, m);
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        ftrl_trainer_t *ftrl_trainer = self->optimizer.ftrl;
        double_matrix_t *weights = logistic_regression_trainer_get_weights(self);
        return ftrl_reg_cost(ftrl_trainer, weights, NULL, m);
    }
    return 0.0;
}


bool logistic_regression_trainer_train_minibatch(logistic_regression_trainer_t *self, feature_count_array *features, cstring_array *labels) {
    double_matrix_t *gradient = self->gradient;

    sparse_matrix_t *x = feature_matrix(self->feature_ids, features);
    if (x == NULL) {
        log_error("x == NULL\n");
        return false;
    }
    uint32_array *y = label_vector(self->label_ids, labels);
    if (y == NULL) {
        log_error("y == NULL\n");
        return false;
    }

    bool ret = false;

    if (!sparse_matrix_add_unique_columns_alias(x, self->unique_columns, self->batch_columns)) {
        log_error("Unique columns failed\n");
        return false;
    }

    if(!double_matrix_resize(gradient, self->batch_columns->n, self->num_labels)) {
        log_error("Gradient resize failed\n");
        return false;
    }

    double_matrix_t *weights = logistic_regression_trainer_get_weights(self);
    if (weights == NULL) {
        log_error("Error getting weights\n");
        return false;
    }
    size_t batch_size = x->m;

    double_matrix_t *p_y = double_matrix_new_aligned(batch_size, self->num_labels, 16);
    if (p_y == NULL) {
        log_error("Error allocating p_y\n");
        return false;
    }

    if (!logistic_regression_gradient(weights, gradient, x, y, p_y)) {
        log_error("Gradient failed\n");
        goto exit_matrices_created;
    }

    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        ret = stochastic_gradient_descent_update_sparse(self->optimizer.sgd, gradient, self->batch_columns, batch_size);        
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        ret = ftrl_update_gradient(self->optimizer.ftrl, gradient, weights, self->batch_columns, batch_size);
        if (!ret) {
            log_error("ftrl_update_gradient failed\n");
        }
    } else {
        ret = false;
    }

exit_matrices_created:
    double_matrix_destroy(p_y);
    uint32_array_destroy(y);
    sparse_matrix_destroy(x);
    return ret;
}

double_matrix_t *logistic_regression_trainer_get_weights(logistic_regression_trainer_t *self) {
    if (self == NULL) return NULL;

    size_t m = self->batch_columns->n;
    size_t n = self->num_labels;
    double_matrix_t *batch_weights = self->batch_weights;
    if (batch_weights == NULL || !double_matrix_resize(batch_weights, m, n)) {
        return NULL;
    }
    double_matrix_zero(batch_weights);

    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        if (self->optimizer.sgd == NULL) return NULL;

        if (!stochastic_gradient_descent_set_regularized_weights(self->optimizer.sgd, self->batch_weights, self->batch_columns)) {
            return NULL;
        }

        return batch_weights;
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        if (self->optimizer.ftrl == NULL) return NULL;

        if (!ftrl_set_weights(self->optimizer.ftrl, batch_weights, self->batch_columns)) {
            return NULL;
        }

        return batch_weights;

    }
    return NULL;
}

double_matrix_t *logistic_regression_trainer_get_regularized_weights(logistic_regression_trainer_t *self) {
    if (self == NULL) return NULL;

    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        if (self->optimizer.sgd == NULL) return NULL;
        return stochastic_gradient_descent_get_weights(self->optimizer.sgd);
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        if (self->optimizer.ftrl == NULL) return NULL;
        if (!ftrl_set_weights(self->optimizer.ftrl, self->batch_weights, NULL)) {
            return NULL;
        }
        return self->batch_weights;
    }
    return NULL;
}

double_matrix_t *logistic_regression_trainer_final_weights(logistic_regression_trainer_t *self) {
    if (self == NULL) return NULL;

    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        if (self->optimizer.sgd == NULL) return NULL;
        double_matrix_t *weights = stochastic_gradient_descent_get_weights(self->optimizer.sgd);
        self->optimizer.sgd->theta = NULL;
        return weights;
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        if (self->optimizer.ftrl == NULL) return NULL;
        return ftrl_weights_finalize(self->optimizer.ftrl);
    }
    return NULL;
}


sparse_matrix_t *logistic_regression_trainer_final_weights_sparse(logistic_regression_trainer_t *self) {
    if (self == NULL) return NULL;

    if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_SGD) {
        if (self->optimizer.sgd == NULL) return NULL;
        return stochastic_gradient_descent_get_weights_sparse(self->optimizer.sgd);
    } else if (self->optimizer_type == LOGISTIC_REGRESSION_OPTIMIZER_FTRL) {
        if (self->optimizer.ftrl == NULL) return NULL;
        return ftrl_weights_finalize_sparse(self->optimizer.ftrl);
    }

    return NULL;
}
