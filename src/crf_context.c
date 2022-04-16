#include "crf_context.h"
#include "float_utils.h"

crf_context_t *crf_context_new(int flag, size_t L, size_t T) {
    crf_context_t *context = malloc(sizeof(crf_context_t));
    if (context == NULL) return NULL;

    context->flag = flag;
    context->num_labels = L;

    context->scale_factor = double_array_new_size_fixed(T);
    if (context->scale_factor == NULL) goto exit_context_created;

    context->row = double_array_new_size_fixed(L);
    if (context->row == NULL) goto exit_context_created;

    context->row_trans = double_array_new_size(L);
    if (context->row_trans == NULL) goto exit_context_created;

    context->alpha_score = double_matrix_new_zeros(T, L);
    if (context->alpha_score == NULL) goto exit_context_created;

    context->beta_score = double_matrix_new_zeros(T, L);
    if (context->beta_score == NULL) goto exit_context_created;

    context->state = double_matrix_new_zeros(T, L);
    if (context->state == NULL) goto exit_context_created;

    context->state_trans = double_matrix_new_zeros(T, L * L);
    if (context->state_trans == NULL) goto exit_context_created;

    context->trans = double_matrix_new_zeros(L, L);
    if (context->trans == NULL) goto exit_context_created;

    if (context->flag & CRF_CONTEXT_VITERBI) {
        context->backward_edges = uint32_matrix_new_zeros(T, L);
        if (context->backward_edges == NULL) goto exit_context_created;
    } else {
        context->backward_edges = NULL;
    }

    if (context->flag & CRF_CONTEXT_MARGINALS) {
#if defined(INTEL_SSE) || defined(ARM_NEON)
        context->exp_state = double_matrix_new_aligned(T, L, 16);
        if (context->exp_state == NULL) goto exit_context_created;
        double_matrix_zero(context->exp_state);
#else
        context->exp_state = double_matrix_new_zeros(T, L);
        if (context->exp_state == NULL) goto exit_context_created;        
#endif

        context->mexp_state = double_matrix_new_zeros(T, L);
        if (context->mexp_state == NULL) goto exit_context_created;

#if defined(INTEL_SSE) || defined(ARM_NEON)
        context->exp_state_trans = double_matrix_new_aligned(T, L * L, 16);
        if (context->exp_state_trans == NULL) goto exit_context_created;
        double_matrix_zero(context->exp_state_trans);
#else
        context->exp_state_trans = double_matrix_new_zeros(T, L * L);
        if (context->exp_state_trans == NULL) goto exit_context_created;
#endif        

        context->mexp_state_trans = double_matrix_new_zeros(T, L * L);
        if (context->mexp_state_trans == NULL) goto exit_context_created;

#if defined(INTEL_SSE) || defined(ARM_NEON)
        context->exp_trans = double_matrix_new_aligned(L, L, 16);
        if (context->exp_trans == NULL) goto exit_context_created;
        double_matrix_zero(context->exp_trans);
#else
        context->exp_trans = double_matrix_new_zeros(L, L);
        if (context->exp_trans == NULL) goto exit_context_created;
#endif

        context->mexp_trans = double_matrix_new_zeros(L, L);
        if (context->mexp_trans == NULL) goto exit_context_created;

    } else {
        context->exp_state = NULL;
        context->mexp_state = NULL;
        context->exp_state_trans = NULL;
        context->mexp_state_trans = NULL;
        context->exp_trans = NULL;
        context->mexp_trans = NULL;
    }

    context->num_items = T;

    return context;

exit_context_created:
    crf_context_destroy(context);
    return NULL;
}

/*
Makes it possible to reuse the same context for many
different sequences.
*/
bool crf_context_set_num_items(crf_context_t *self, size_t T) {
    const size_t L = self->num_labels;

    if (!double_array_resize_fixed(self->scale_factor, T)) {
        return false;
    }
    if (!double_array_resize_fixed(self->row, L)) {
        return false;
    }

    if (!double_matrix_resize(self->alpha_score, T, L) ||
        !double_matrix_resize(self->beta_score, T, L) ||
        !double_matrix_resize(self->state, T, L) ||
        !double_matrix_resize(self->state_trans, T, L * L)) {
            return false;
    }

    double_matrix_zero(self->alpha_score);
    double_matrix_zero(self->beta_score);
    double_matrix_zero(self->state);
    double_matrix_zero(self->state_trans);

    if (self->flag & CRF_CONTEXT_VITERBI && self->backward_edges != NULL) {
        if (!uint32_matrix_resize(self->backward_edges, T, L)) {
            return false;
        }

        uint32_matrix_zero(self->backward_edges);
    }

    if (self->flag & CRF_CONTEXT_MARGINALS &&
        (
#if defined(INTEL_SSE) || defined(ARM_NEON)
            !double_matrix_resize_aligned(self->exp_state, T, L, 16) ||
#else
            !double_matrix_resize(self->exp_state, T, L) ||
#endif
            !double_matrix_resize(self->mexp_state, T, L) ||
#if defined(INTEL_SSE) || defined(ARM_NEON)
            !double_matrix_resize_aligned(self->exp_state_trans, T, L * L, 16) ||
#else
            !double_matrix_resize(self->exp_state_trans, T, L * L) ||            
#endif
            !double_matrix_resize(self->mexp_state_trans, T, L * L)
        )) {
        return false;

        double_matrix_zero(self->exp_state);
        double_matrix_zero(self->mexp_state);
        double_matrix_zero(self->exp_state_trans);
        double_matrix_zero(self->mexp_state_trans);
    }

    self->num_items = T;

    return true;
}

void crf_context_destroy(crf_context_t *self) {
    if (self == NULL) return;

    if (self->scale_factor != NULL) {
        double_array_destroy(self->scale_factor);
    }

    if (self->row != NULL) {
        double_array_destroy(self->row);
    }

    if (self->row_trans != NULL) {
        double_array_destroy(self->row_trans);
    }

    if (self->alpha_score != NULL) {
        double_matrix_destroy(self->alpha_score);
    }

    if (self->beta_score != NULL) {
        double_matrix_destroy(self->beta_score);
    }

    if (self->state != NULL) {
        double_matrix_destroy(self->state);
    }

    if (self->exp_state != NULL) {
#if defined(INTEL_SSE) || defined(ARM_NEON)
        double_matrix_destroy_aligned(self->exp_state);
#else
        double_matrix_destroy(self->exp_state);
#endif
    }

    if (self->mexp_state != NULL) {
        double_matrix_destroy(self->mexp_state);
    }

    if (self->state_trans != NULL) {
        double_matrix_destroy(self->state_trans);
    }

    if (self->exp_state_trans != NULL) {
#if defined(INTEL_SSE) || defined(ARM_NEON)
        double_matrix_destroy_aligned(self->exp_state_trans);
#else
        double_matrix_destroy(self->exp_state_trans);
#endif
    }

    if (self->mexp_state_trans != NULL) {
        double_matrix_destroy(self->mexp_state_trans);
    }

    if (self->trans != NULL) {
        double_matrix_destroy(self->trans);
    }

    if (self->exp_trans != NULL) {
#if defined(INTEL_SSE) || defined(ARM_NEON)
        double_matrix_destroy_aligned(self->exp_trans);
#else
        double_matrix_destroy(self->exp_trans);
#endif
    }

    if (self->mexp_trans != NULL) {
        double_matrix_destroy(self->mexp_trans);
    }

    if (self->backward_edges != NULL) {
        uint32_matrix_destroy(self->backward_edges);
    }

    free(self);
}

void crf_context_reset(crf_context_t *context, int flag) {
    const size_t T = context->num_items;
    const size_t L = context->num_labels;

    if (flag & CRF_CONTEXT_RESET_STATE) {
        double_matrix_zero(context->state);
    }

    if (flag & CRF_CONTEXT_RESET_STATE_TRANS) {
        double_matrix_zero(context->state_trans);
        double_matrix_zero(context->trans);
    }

    if (context->flag & CRF_CONTEXT_MARGINALS) {
        double_matrix_zero(context->mexp_state);
        double_matrix_zero(context->mexp_state_trans);
        double_matrix_zero(context->mexp_trans);
        context->log_norm = 0;
    }
}

static inline double *state_trans_matrix_get_row(crf_context_t *self, double_matrix_t *matrix, size_t t, size_t i) {
    double *row = double_matrix_get_row(matrix, t);
    return row + i * self->num_labels;
}

inline double *alpha_score(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->alpha_score, t);
}

inline double *beta_score(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->beta_score, t);
}

inline double *state_score(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->state, t);
}

inline double *state_trans_score(crf_context_t *self, size_t t, size_t i) {
    return state_trans_matrix_get_row(self, self->state_trans, t, i);
}

inline double *state_trans_score_all(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->state_trans, t);
}

inline double *trans_score(crf_context_t *self, size_t i) {
    return double_matrix_get_row(self->trans, i);
}

inline double *exp_state_score(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->exp_state, t);
}

inline double *exp_state_trans_score(crf_context_t *self, size_t t, size_t i) {
    return state_trans_matrix_get_row(self, self->exp_state_trans, t, i);
}

inline double *exp_state_trans_score_all(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->exp_state_trans, t);
}

inline double *exp_trans_score(crf_context_t *self, size_t i) {
    return double_matrix_get_row(self->exp_trans, i);
}

inline double *state_mexp(crf_context_t *self, size_t t) {
    return double_matrix_get_row(self->mexp_state, t);
}

inline double *state_trans_mexp(crf_context_t *self, size_t t, size_t i) {
    return state_trans_matrix_get_row(self, self->mexp_state_trans, t, i);
}

inline double *trans_mexp(crf_context_t *self, size_t i) {
    return double_matrix_get_row(self->mexp_trans, i);
}

inline uint32_t *backward_edge_at(crf_context_t *self, size_t t) {
    return uint32_matrix_get_row(self->backward_edges, t);
}

bool crf_context_exp_state(crf_context_t *self) {
    if (!double_matrix_copy(self->state, self->exp_state)) {
        return false;
    }

    double_matrix_exp(self->exp_state);
    return true;
}

bool crf_context_exp_state_trans(crf_context_t *self) {
    if (!double_matrix_copy(self->state_trans, self->exp_state_trans)) {
        return false;
    }

    double_matrix_exp(self->exp_state_trans);
    return true;
}

bool crf_context_exp_trans(crf_context_t *self) {
    if (!double_matrix_copy(self->trans, self->exp_trans)) {
        return false;
    }

    double_matrix_exp(self->exp_trans);
    return true;
}


void crf_context_alpha_score(crf_context_t *self) {
    double *scale = self->scale_factor->a;

    const double *prev = NULL;
    const double *trans = NULL;
    const double *state = NULL;
    const double *state_trans = NULL;

    const size_t T = self->num_items;
    const size_t L = self->num_labels;

    /* Compute the alpha scores on nodes 0, *).
       alpha[0][j] = state[0][j]

       At this point we have no transition features.
    */
    double *cur = alpha_score(self, 0);
    state = exp_state_score(self, 0);
    double_array_raw_copy(cur, state, L);
    double sum = double_array_sum(cur, L);
    double scale_t = !double_equals(sum, 0.) ? 1. / sum : 1.;
    scale[0] = scale_t;
    double_array_mul(cur, scale[0], L);

    /* Compute the alpha scores on nodes (t, *).
       alpha[t][j] = state[t][j] * \sum_{i} alpha[t-1][i] * state_trans[t][i][j] * trans[i][j]
    */
    for (size_t t = 1; t < T; t++) {
        prev = alpha_score(self, t - 1);
        cur = alpha_score(self, t);
        state = exp_state_score(self, t);

        double_array_zero(cur, L);
        for (size_t i = 0; i < L; i++) {
            trans = exp_trans_score(self, i);
            state_trans = exp_state_trans_score(self, t, i);
            for (size_t j = 0; j < L; j++) {
                cur[j] += prev[i] * trans[j] * state_trans[j];
            }
        }

        double_array_mul_array(cur, state, L);
        sum = double_array_sum(cur, L);
        scale[t] = scale_t = !double_equals(sum, 0.) ? 1. / sum : 1.;
        double_array_mul(cur, scale_t, L);
    }

    /* Compute the logarithm of the normalization factor here.
       norm = 1. / (C[0] * C[1] ... * C[T-1])
       log(norm) = - \sum_{t = 0}^{T-1} log(C[t])
    */
    self->log_norm = -double_array_sum_log(scale, T);
}

void crf_context_beta_score(crf_context_t *self) {
    double *cur = NULL;
    double *row = self->row->a;
    double *row_trans = self->row_trans->a;

    const double *next = NULL;
    const double *state = NULL;
    const double *state_trans = NULL;
    const double *trans = NULL;

    const size_t T = self->num_items;
    const size_t L = self->num_labels;

    double *scale = self->scale_factor->a;

    /* Compute the beta scores at (T-1, *). */
    cur = beta_score(self, T - 1);

    double scale_t = scale[T - 1];
    double_array_set(cur, scale_t, L);

    /* Compute the beta scores at (t, *). */
    for (ssize_t t = T - 2; t >= 0; t--) {
        cur = beta_score(self, t);
        next = beta_score(self, t + 1);
        state = exp_state_score(self, t + 1);

        double_array_raw_copy(row, next, L);
        double_array_mul_array(row, state, L);

        /* Compute the beta score at (t, i). */
        for (int i = 0; i < L; i++) {
            trans = exp_trans_score(self, i);
            double_array_raw_copy(row_trans, row, L);
            double_array_mul_array(row_trans, trans, L);
            state_trans = exp_state_trans_score(self, t + 1, i);
            cur[i] = double_array_dot(state_trans, row_trans, L);
        }

        scale_t = scale[t];
        double_array_mul(cur, scale_t, L);
        
    }
}

void crf_context_marginals(crf_context_t *self) {
    const size_t T = self->num_items;
    const size_t L = self->num_labels;

    double *scale = self->scale_factor->a;

    /*
        Compute the model expectations of states.
            p(t,i) = fwd[t][i] * bwd[t][i] / norm
                   = (1. / C[t]) * fwd'[t][i] * bwd'[t][i]
     */
    int t;

    for (t = 0; t < T; t++) {
        double *forward = alpha_score(self, t);
        double *backward = beta_score(self, t);
        double *prob = state_mexp(self, t);

        double_array_raw_copy(prob, forward, L);
        double_array_mul_array(prob, backward, L);
        double scale_t = scale[t];
        double_array_div(prob, scale_t, L);
    }

    /*
        Compute the model expectations of transitions.
            p(t,i,t+1,j)
                = fwd[t][i] * state[t+1][j] * edge[i][j] * state_edge[t+1][i][j] * bwd[t+1][j] / norm
                = (fwd'[t][i] / (C[0] ... C[t])) * state[t+1][j] * edge[i][j] * state_edge[t+1][i][j] * (bwd'[t+1][j] / (C[t+1] ... C[T-1])) * (C[0] * ... * C[T-1])
                = fwd'[t][i] * state[t+1][j] * edge[i][j] * state_edge[t+1][i][j] * bwd'[t+1][j]
        The model expectation of a transition (i -> j) is the sum of the marginal
        probabilities p(t,i,t+1,j) over t.
     */

    for (t = 0; t < T - 1; t++) {
        double *forward  = alpha_score(self, t);
        double *state = exp_state_score(self, t + 1);
        double *backward = beta_score(self, t + 1);
        double *row = self->row->a;

        double_array_raw_copy(row, backward, L);
        double_array_mul_array(row, state, L);

        for (int i = 0; i < L; i++) {
            double *edge = exp_trans_score(self, i);
            double *edge_state = exp_state_trans_score(self, t + 1, i);
            double *prob = state_trans_mexp(self, t + 1, i);

            for (int j = 0; j < L; j++) {
                prob[j] += forward[i] * edge[j] * edge_state[j] * row[j];
            }
        }
    }
}

double crf_context_marginal_point(crf_context_t *self, uint32_t l, uint32_t t) {
    double *forward = alpha_score(self, t);
    double *backward = beta_score(self, t);
    return forward[l] * backward[l] / self->scale_factor->a[t];
}

double crf_context_marginal_path(crf_context_t *self, const uint32_t *path, size_t begin, size_t end) {
    /*
        Compute the marginal probability of a (partial) path.
            a = path[begin], b = path[begin+1], ..., y = path[end-2], z = path[end-1]
            fwd[begin][a] = (fwd'[begin][a] / (C[0] ... C[begin])
            bwd[end-1][z] = (bwd'[end-1][z] / (C[end-1] ... C[T-1]))
            norm = 1 / (C[0] * ... * C[T-1])
            p(a, b, ..., z)
                = fwd[begin][a] * state_edge[begin+1][a * L + b] * edge[a][b] * state[begin+1][b] * ... * edge[y][z] * state_edge[end-1][y][z] * state[end-1][z] * bwd[end-1][z] / norm
                = fwd'[begin][a] * state_edge[begin+1][a * L + b] * edge[a][b] * state[begin+1][b] * ... * edge[y][z] * state_edge[end-1][y][z] * state[end-1][z] * bwd'[end-1][z] * (C[begin+1] * ... * C[end-2])
    */

    double *forward = alpha_score(self, begin);
    double *backward = beta_score(self, end - 1);

    double prob = forward[path[begin]] * backward[path[end]];

    for (int t = begin; t < end - 1; t++) {
        double *state = exp_state_score(self, t + 1);
        double *edge = exp_trans_score(self, (size_t)path[t]);
        double *edge_state = exp_state_trans_score(self, t + 1, (size_t)path[t]);

        prob *= (edge[path[t+1]] * edge_state[path[t + 1]] * state[path[t + 1]] * self->scale_factor->a[t]);
    }

    return prob;
}

double crf_context_score(crf_context_t *self, const uint32_t *labels) {
    double ret = 0.0;
    const size_t T = self->num_items;
    const size_t L = self->num_labels;

    const double *cur = NULL;

    const double *state = NULL;
    const double *state_trans = NULL;
    const double *trans = NULL;

    uint32_t i = labels[0];
    state = state_score(self, 0);
    ret = state[i];

    for (size_t t = 1; t < T; t++) {
        uint32_t j = labels[t];
        state = state_score(self,  t);
        state_trans = state_trans_score(self, t, (size_t)i);
        trans = trans_score(self, (size_t)i);

        ret += state[j] + state_trans[j] + trans[j];
        i = j;
    }

    return ret;
}

double crf_context_lognorm(crf_context_t *self) {
    return self->log_norm;
}

double crf_context_viterbi(crf_context_t *self, uint32_t *labels) {
    uint32_t *back = NULL;

    double max_score = -DBL_MAX;
    double score;
    ssize_t argmax_score = -1;
    double *cur = NULL;
    const double *prev = NULL;
    const double *state = NULL;
    const double *state_trans = NULL;
    const double *trans = NULL;

    const size_t T = self->num_items;
    if (T == 0) {
        return max_score;
    }
    const size_t L = self->num_labels;

    // This function assumes state and trans scores to be in the logarithm domain.

    /* Compute the scores at (0, *).
       This is just the state score. 

       Remember that alpha_score and state_score, etc. are
       just returning matrix row pointers so the actual
       values of the scores are computed beforehand 
     */
    cur = alpha_score(self, 0);
    state = state_score(self, 0);

    double_array_raw_copy(cur, state, L);

    int i, j, t;

    for (t = 1; t < T; t++) {
        prev = alpha_score(self, t - 1);
        cur = alpha_score(self, t);
        state = state_score(self, t);
        back = backward_edge_at(self, t);

        /*
            Loop through all the labels we could transition to,
            then do an inner loop of all the labels we could have
            transitioned out of (i.e. don't take the last prediction
            as given). This allows CRFs to find a globally optimal path
            that might not have been obvious with a simple greedy 
            left-to-right algorithm.

            This algorithm is only quadratic in L (# of labels, usually small)
        */

        for (j = 0; j < L; j++) {
            max_score = -DBL_MAX;
            argmax_score = -1;

            for (i = 0; i < L; i++) {
                state_trans = state_trans_score(self, t, i);
                trans = trans_score(self, i);
                score = prev[i] + state_trans[j] + trans[j];

                /* Store this path if it has the maximum score. */
                if (max_score < score) {
                    max_score = score;
                    argmax_score = i;
                }
            }

            if (argmax_score >= 0) {
                /* Backward link (#t, #j) -> (#t-1, #i). */
                back[j] = argmax_score;
            }

            /* Add the state score on (t, j). */
            cur[j] = max_score + state[j];
        }
    }

    /* Find the node (#T, #i) that reaches EOS with the maximum score. */
    max_score = -DBL_MAX;
    argmax_score = -1;

    prev = alpha_score(self, T - 1);

    /* Set a score for T-1 to be overwritten later. Just in case we don't
       end up with something beating -DBL_MAX. */
    labels[T - 1] = 0;
    for (i = 0; i < L; i++) {
        if (prev[i] > max_score) {
            max_score = prev[i];
            argmax_score = i;
        }
    }

    if (argmax_score >= 0) {
        labels[T - 1] = argmax_score;  /* Tag the item #T. */
    }

    /* Tag labels by tracing the backward links. */
    for (t = T - 2; t >= 0; t--) {
        back = backward_edge_at(self, t + 1);
        labels[t] = back[labels[t + 1]];
    }

    return max_score;
}
