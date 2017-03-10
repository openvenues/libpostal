#ifndef CRF_CONTEXT_H
#define CRF_CONTEXT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "collections.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "collections.h"
#include "matrix.h"

/**
 * Functionality flags for contexts.
 *  @see    crf_context_new().
 */
enum {
    CRF_CONTEXT_BASE       = 0x01,
    CRF_CONTEXT_VITERBI    = 0x01,
    CRF_CONTEXT_MARGINALS  = 0x02,
    CRF_CONTEXT_ALL        = 0xFF,
};

/**
 * Reset flags.
 *  @see    crf_context_reset().
 */

#define CRF_CONTEXT_RESET_STATE (1 << 0)
#define CRF_CONTEXT_RESET_STATE_TRANS (1 << 1)
#define CRF_CONTEXT_RESET_ALL ((1 << 16) - 1)


#define CRF_CONTEXT_DEFAULT_NUM_ITEMS 10

typedef struct crf_context {
    /**
     * Flag specifying the functionality
     */
    int flag;
    /**
     * The total number of distinct lables (L)
     */
    size_t num_labels;
    /**
     * The number of items (T) in the instance
     */
    size_t num_items;
    /**
     * Logarithm of the normalization factor for the instance.
     * This is equivalent to the total scores of all paths in the lattice.
     */
    double log_norm;

    /**
     * State scores.
     * This is a [T][L] matrix whose element [t][l] presents total score
     * of state features associating label l at t
     */
    double_matrix_t *state;

    /**
     * State-transition scores.
     * This is a [T][L * L] matrix whose element [t][i * L + j] represents the
     * score of state features associated with label i and j
     */
    double_matrix_t *state_trans;

    /**
     * Transition scores.
     * This is a [L][L] matrix whose element [i][j] represents the
     * score of transition features associating labels i and j
     */
    double_matrix_t *trans;

    /**
     * Alpha score matrix.
     * This is a [T][L] matrix whose element [t][l] presents the total
     * score of paths starting at BOS and arriving at (t, l).
     */
    double_matrix_t *alpha_score;

    /**
     * Beta score matrix.
     * This is a [T][L] matrix whose element [t][l] presents the total
     * score of paths starting at (t, l) and arriving at EOS
     */
    double_matrix_t *beta_score;

    /**
     * Scale factor vector.
     * This is a [T] vector whose element [t] presents the scaling
     * coefficient for the alpha score and beta score.
     */
    double_array *scale_factor;

    /**
     * Row vector (work space).
     * This is a [T] vector used internally for a work space.
     */
    double_array *row;

    /**
     * Row vector for the transitions (work space).
     * This is a [T] vector used internally for a work space.
     */
    double_array *row_trans;

    /**
     * This is a [T][L] matrix whose element [t][j] represents the label
     * that yields the maximum score to arrive at (t, j).
     * This member is available only with CRF_CONTEXT_VITERBI flag enabled.
     */
    uint32_matrix_t *backward_edges;

    /**
     * Exponents of state scores.
     * This is a [T][L] matrix whose element [t][l] represents the exponent
     * of the total score of state features associating label l at t.
     * This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *exp_state;

    /**
     * Exponents of state-transition scores.
     * This is a [T][L * L] matrix whose element [t][i * L + j] represents the
     * exponent of the total score of state features associated with label i and j
     * at t. This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *exp_state_trans;

    /**
     * Exponents of transition scores.
     * This is a [L][L] matrix whose element [i][j] represents the exponent
     * of the total score of transition features associating labels i and j.
     * This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *exp_trans;

    /**
     * Model expectations of states.
     * This is a [T][L] matrix whose element [t][l] presents the model 
     * expectation (marginal probability) of the state (t,l)
     * This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *mexp_state;

    /**
     * Model expectations of state transitions.
     * This is a [T][L * L] matrix whose element [t][i * L + j] presents the model 
     * expectation (marginal probability) of the state t and transition (i->j)
     * This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *mexp_state_trans;

    /**
     * Model expectations of transitions.
     * This is a [L][L] matrix whose element [i][j] presents the model
     * expectation of the transition (i->j).
     * This member is available only with CRF_CONTEXT_MARGINALS flag.
     */
    double_matrix_t *mexp_trans;

} crf_context_t;

double *alpha_score(crf_context_t *context, size_t t);
double *beta_score(crf_context_t *context, size_t t);
double *state_score(crf_context_t *context, size_t t);
double *state_trans_score(crf_context_t *context, size_t t, size_t i);
double *state_trans_score_all(crf_context_t *self, size_t t);
double *trans_score(crf_context_t *context, size_t i);
double *exp_state_score(crf_context_t *context, size_t t);
double *exp_state_trans_score(crf_context_t *context, size_t t, size_t i);
double *exp_trans_score(crf_context_t *context, size_t i);
double *state_mexp(crf_context_t *context, size_t t);
double *state_trans_mexp(crf_context_t *context, size_t t, size_t i);
double *trans_mexp(crf_context_t *context, size_t i);
uint32_t *backward_edge_at(crf_context_t *context, size_t t);

crf_context_t *crf_context_new(int flag, size_t L, size_t T);
bool crf_context_set_num_items(crf_context_t *self, size_t T);
void crf_context_destroy(crf_context_t *self);
void crf_context_reset(crf_context_t *self, int flag);
bool crf_context_exp_state(crf_context_t *self);
bool crf_context_exp_transition(crf_context_t *self);

void crf_context_alpha_score(crf_context_t *self);
void crf_context_beta_score(crf_context_t *self);
void crf_context_marginals(crf_context_t *self);

double crf_context_marginal_point(crf_context_t *self, uint32_t l, uint32_t t);
double crf_context_marginal_path(crf_context_t *self, const uint32_t *path, size_t begin, size_t end);
double crf_context_score(crf_context_t *self, const uint32_t *labels);
double crf_context_lognorm(crf_context_t *self);
double crf_context_viterbi(crf_context_t *self, uint32_t *labels);

#endif