#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "greatest.h"
#include "../src/float_utils.c"
#include "../src/crf_context.c"

SUITE(libpostal_crf_context_tests);

static greatest_test_res check_values(double cv, double tv) {
    ASSERT_IN_RANGE(cv, tv, 1e-9);
    PASS();
}

static greatest_test_res check_matrix_size(double_matrix_t *x, size_t m, size_t n) {
    ASSERT(x);
    ASSERT_EQ(x->m, m);
    ASSERT_EQ(x->n, n);
    PASS();
}

TEST test_crf_context(void) {
    int y1, y2, y3;
    double norm = 0;

    const size_t L = 3;
    const size_t T = 3;

    crf_context_t *ctx = crf_context_new(CRF_CONTEXT_ALL, L, 1);
    ASSERT(ctx != NULL);

    const size_t T_large = 100; 

    bool ret = crf_context_set_num_items(ctx, T_large);
    ASSERT(ret);

    check_matrix_size(ctx->state, T_large, L);
    check_matrix_size(ctx->exp_state, T_large, L);
    check_matrix_size(ctx->state_trans, T_large, L * L);
    check_matrix_size(ctx->exp_state_trans, T_large, L * L);
    check_matrix_size(ctx->trans, L, L);
    check_matrix_size(ctx->exp_trans, L, L);

    ret = crf_context_set_num_items(ctx, T);
    ASSERT(ret);

    check_matrix_size(ctx->state, T, L);
    check_matrix_size(ctx->exp_state, T, L);
    check_matrix_size(ctx->state_trans, T, L * L);
    check_matrix_size(ctx->exp_state_trans, T, L * L);
    check_matrix_size(ctx->trans, L, L);
    check_matrix_size(ctx->exp_trans, L, L);

    double *state_trans = NULL;
    double *state = NULL;
    double *trans = NULL;
    double scores[T][L][L];
    uint32_t labels[L];

    /* Initialize the state scores. */
    state = state_score(ctx, 0);
    state[0] = .4;    state[1] = .5;    state[2] = .1;
    state = state_score(ctx, 1);
    state[0] = .4;    state[1] = .1;    state[2] = .5;
    state = state_score(ctx, 2);
    state[0] = .4;    state[1] = .1;    state[2] = .5;

    printf("state\n");

    /* Initialize the state scores. */
    state_trans = state_trans_score(ctx, 0, 0);
    state_trans[0] = .4;    state_trans[1] = .2;    state_trans[2] = .5;
    state_trans = state_trans_score(ctx, 0, 1);
    state_trans[0] = .4;    state_trans[1] = .2;    state_trans[2] = .5;
    state_trans = state_trans_score(ctx, 0, 2);
    state_trans[0] = .4;    state_trans[1] = .2;    state_trans[2] = .5;
    state_trans = state_trans_score(ctx, 1, 0);
    state_trans[0] = .3;    state_trans[1] = .1;    state_trans[2] = .6;
    state_trans = state_trans_score(ctx, 1, 1);
    state_trans[0] = .5;    state_trans[1] = .1;    state_trans[2] = .3;
    state_trans = state_trans_score(ctx, 1, 2);
    state_trans[0] = .4;    state_trans[1] = .2;    state_trans[2] = .4;
    state_trans = state_trans_score(ctx, 2, 0);
    state_trans[0] = .3;    state_trans[1] = .1;    state_trans[2] = .6;
    state_trans = state_trans_score(ctx, 2, 1);
    state_trans[0] = .5;    state_trans[1] = .1;    state_trans[2] = .3;
    state_trans = state_trans_score(ctx, 2, 2);
    state_trans[0] = .4;    state_trans[1] = .2;    state_trans[2] = .4;

    printf("state_trans\n");

    trans = trans_score(ctx, 0);
    trans[0] = .3;    trans[1] = .1;    trans[2] = .4;
    trans = trans_score(ctx, 1);
    trans[0] = .6;    trans[1] = .2;    trans[2] = .1;
    trans = trans_score(ctx, 2);
    trans[0] = .5;    trans[1] = .2;    trans[2] = .1;

    printf("trans\n");

    crf_context_exp_state(ctx);
    printf("exp state\n");
    crf_context_exp_state_trans(ctx);
    printf("exp state_trans\n");
    crf_context_exp_trans(ctx);
    printf("exp trans\n");

    crf_context_alpha_score(ctx);
    printf("alpha\n");

    crf_context_beta_score(ctx);
    printf("beta\n");

    /* Compute the score of every label sequence. */
    for (y1 = 0; y1 < T; y1++) {
        double s1 = exp_state_score(ctx, 0)[y1];
        for (y2 = 0; y2 < L; y2++) {
            double s2 = s1;
            s2 *= exp_state_trans_score(ctx, 1, y1)[y2];
            s2 *= exp_trans_score(ctx, y1)[y2];
            s2 *= exp_state_score(ctx, 1)[y2];
            for (y3 = 0; y3 < L; y3++) {
                double s3 = s2;
                s3 *= exp_state_trans_score(ctx, 2, y2)[y3];
                s3 *= exp_trans_score(ctx, y2)[y3];
                s3 *= exp_state_score(ctx, 2)[y3];
                scores[y1][y2][y3] = s3;
            }
        }
    }

    /* Compute the partition factor. */
    norm = 0.;
    for (y1 = 0; y1 < T; y1++) {
        for (y2 = 0; y2 < L; y2++) {
            for (y3 = 0; y3 < L; y3++) {
                norm += scores[y1][y2][y3];
            }
        }
    }

    /* Check the partition factor. */
    printf("Check for the partition factor...\n");
    CHECK_CALL(check_values(exp(ctx->log_norm), norm));

    /* Compute the sequence probabilities. */
    for (y1 = 0; y1 < T; y1++) {
        for (y2 = 0; y2 < L; y2++) {
            for (y3 = 0; y3 < L; y3++) {
                double logp;

                labels[0] = y1;
                labels[1] = y2;
                labels[2] = y3;
                logp = crf_context_score(ctx, labels) - crf_context_lognorm(ctx);
                printf("Check for the sequence %d-%d-%d...\n", y1, y2, y3);
                CHECK_CALL(check_values(exp(logp), scores[y1][y2][y3] / norm));
            }
        }
    }

    /* Compute the marginal probability at t=0 */
    for (y1 = 0; y1 < T; y1++) {
        double a, b, c, s = 0.;
        for (y2 = 0; y2 < L; y2++) {
            for (y3 = 0; y3 < L; y3++) {
                s += scores[y1][y2][y3];
            }
        }

        a = alpha_score(ctx, 0)[y1];
        b = beta_score(ctx, 0)[y1];
        c = 1. / ctx->scale_factor->a[0];

        printf("Check for the marginal probability (0,%d)...\n", y1);
        CHECK_CALL(check_values(a * b * c, s / norm));
    }

    /* Compute the marginal probability at t=1 */
    for (y2 = 0; y2 < L; y2++) {
        double a, b, c, s = 0.;
        for (y1 = 0; y1 < T; y1++) {
            for (y3 = 0; y3 < L; y3++) {
                s += scores[y1][y2][y3];
            }
        }

        a = alpha_score(ctx, 1)[y2];
        b = beta_score(ctx, 1)[y2];
        c = 1. / ctx->scale_factor->a[1];

        printf("Check for the marginal probability (1,%d)...\n", y2);
        CHECK_CALL(check_values(a * b * c, s / norm));
    }

    /* Compute the marginal probability at t=2 */
    for (y3 = 0; y3 < L; y3++) {
        double a, b, c, s = 0.;
        for (y1 = 0; y1 < T; y1++) {
            for (y2 = 0; y2 < L; y2++) {
                s += scores[y1][y2][y3];
            }
        }

        a = alpha_score(ctx, 2)[y3];
        b = beta_score(ctx, 2)[y3];
        c = 1. / ctx->scale_factor->a[2];

        printf("Check for the marginal probability (2,%d)...\n", y3);
        CHECK_CALL(check_values(a * b * c, s / norm));
    }

    /* Compute the marginal probabilities of transitions. */
    for (y1 = 0; y1 < T; y1++) {
        for (y2 = 0; y2 < L; y2++) {
            double a, b, s, st, t, p = 0.;
            for (y3 = 0; y3 < L; y3++) {
                p += scores[y1][y2][y3];
            }

            a = alpha_score(ctx, 0)[y1];
            b = beta_score(ctx, 1)[y2];
            s = exp_state_score(ctx, 1)[y2];
            st = exp_state_trans_score(ctx, 1, y1)[y2];
            t = exp_trans_score(ctx, y1)[y2];

            printf("Check for the marginal probability (0,%d)-(1,%d)...\n", y1, y2);
            CHECK_CALL(check_values(a * t * st * s * b, p / norm));
        }
    }

    for (y2 = 0; y2 < L; y2++) {
        for (y3 = 0; y3 < L; y3++) {
            double a, b, s, st, t, p = 0.;
            for (y1 = 0; y1 < T; y1++) {
                p += scores[y1][y2][y3];
            }

            a = alpha_score(ctx, 1)[y2];
            b = beta_score(ctx, 2)[y3];
            s = exp_state_score(ctx, 2)[y3];
            st = exp_state_trans_score(ctx, 2, y2)[y3];
            t = exp_trans_score(ctx, y2)[y3];

            printf("Check for the marginal probability (1,%d)-(2,%d)...\n", y2, y3);
            CHECK_CALL(check_values(a * t * st * s * b, p / norm));
        }
    }

    double viterbi = crf_context_viterbi(ctx, labels);
    printf("viterbi score=%f\n", viterbi);
    for (int i = 0; i < L; i++) {
        printf("label[%d]=%d\n", i, labels[i]);
    }

    crf_context_destroy(ctx);

    PASS();
}


SUITE(libpostal_crf_context_tests) {

    RUN_TEST(test_crf_context);

}
