#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "vector.h"

#define ks_lt_index(a, b) ((a).value < (b).value)

#if   defined(INTEL_SSE)
#include <emmintrin.h>
#elif defined(ARM_NEON)
#include "sse2neon.h"
#endif

/*
    Useful macro definitions for memory alignment:
        http://homepage1.nifty.com/herumi/prog/gcc-and-vc.html#MIE_ALIGN
 */

#ifdef _MSC_VER
#define MIE_ALIGN(x) __declspec(align(x))
#else
#define MIE_ALIGN(x) __attribute__((aligned(x)))
#endif

#define CONST_128D(var, val) \
    MIE_ALIGN(16) static const double var[2] = {(val), (val)}


#define VECTOR_INIT_NUMERIC(name, type, unsigned_type, type_abs)                                        \
    __VECTOR_BASE(name, type)                                                                           \
    __VECTOR_DESTROY(name, type)                                                                        \
                                                                                                        \
    static inline void name##_zero(type *array, size_t n) {                                             \
        memset(array, 0, n * sizeof(type));                                                             \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_raw_copy(type *dst, const type *src, size_t n) {                          \
        memcpy(dst, src, n * sizeof(type));                                                             \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_set(type *array, type value, size_t n) {                                  \
        for (size_t i = 0; i < n; i++) {                                                                \
            array[i] = value;                                                                           \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline name *name##_new_value(size_t n, type value) {                                        \
        name *vector = name##_new_size(n);                                                              \
        if (vector == NULL) return NULL;                                                                \
        name##_set(vector->a, n, (type)value);                                                          \
        vector->n = n;                                                                                  \
        return vector;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline name *name##_new_ones(size_t n) {                                                     \
        return name##_new_value(n, (type)1);                                                            \
    }                                                                                                   \
                                                                                                        \
    static inline name *name##_new_zeros(size_t n) {                                                    \
        name *vector = name##_new_size(n);                                                              \
        if (vector == NULL) return NULL;                                                                \
        name##_zero(vector->a, n);                                                                      \
        vector->n = n;                                                                                  \
        return vector;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline bool name##_resize_fill_zeros(name *self, size_t n) {                             \
        size_t old_n = self->n;                                                                         \
        bool ret = name##_resize(self, n);                                                              \
        if (ret && n > old_n) {                                                                         \
            memset(self->a + old_n, 0, (n - old_n) * sizeof(type));                                \
        }                                                                                               \
        return ret;                                                                                     \
    }                                                                                                   \
                                                                                                        \
    static inline bool name##_resize_aligned_fill_zeros(name *self, size_t n, size_t alignment) {   \
        size_t old_n = self->n;                                                                         \
        bool ret = name##_resize_aligned(self, n, alignment);                                           \
        if (ret && n > old_n) {                                                                         \
            memset(self->a + old_n, 0, (n - old_n) * sizeof(type));                                \
        }                                                                                               \
        return ret;                                                                                     \
    }                                                                                                   \
                                                                                                        \
    static inline type name##_max(type *array, size_t n) {                                              \
        if (n < 1) return (type) 0;                                                                     \
        type val = array[0];                                                                            \
        type max_val = val;                                                                             \
        for (size_t i = 1; i < n; i++) {                                                                \
            val = array[i];                                                                             \
            if (val > max_val) max_val = val;                                                           \
        }                                                                                               \
        return max_val;                                                                                 \
    }                                                                                                   \
                                                                                                        \
    static inline type name##_min(type *array, size_t n) {                                              \
        if (n < 1) return (type) 0;                                                                     \
        type val = array[0];                                                                            \
        type min_val = val;                                                                             \
        for (size_t i = 1; i < n; i++) {                                                                \
            val = array[i];                                                                             \
            if (val < min_val) min_val = val;                                                           \
        }                                                                                               \
        return min_val;                                                                                 \
    }                                                                                                   \
                                                                                                        \
    static inline int64_t name##_argmax(type *array, size_t n) {                                        \
        if (n < 1) return -1;                                                                           \
        type val = array[0];                                                                            \
        type max_val = val;                                                                             \
        int64_t argmax = 0;                                                                             \
        for (size_t i = 0; i < n; i++) {                                                                \
            val = array[i];                                                                             \
            if (val > max_val) {                                                                        \
                max_val = val;                                                                          \
                argmax = i;                                                                             \
            }                                                                                           \
        }                                                                                               \
        return argmax;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline int64_t name##_argmin(type *array, size_t n) {                                        \
        if (n < 1) return (type) -1;                                                                    \
        type val = array[0];                                                                            \
        type min_val = val;                                                                             \
        int64_t argmin = 0;                                                                             \
        for (size_t i = 1; i < n; i++) {                                                                \
            val = array[i];                                                                             \
            if (val < min_val) {                                                                        \
                min_val = val;                                                                          \
                argmin = i;                                                                             \
            }                                                                                           \
        }                                                                                               \
        return argmin;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    typedef struct type##_index {                                                                       \
        size_t index;                                                                                   \
        type value;                                                                                     \
    } type##_index_t;                                                                                   \
                                                                                                        \
    KSORT_INIT_GENERIC(type)                                                                            \
    KSORT_INIT(type##_indices, type##_index_t, ks_lt_index)                                             \
                                                                                                        \
    static inline void name##_sort(type *array, size_t n) {                                             \
        ks_introsort(type, n, array);                                                                   \
    }                                                                                                   \
                                                                                                        \
    static inline size_t *name##_argsort(type *array, size_t n) {                                       \
        type##_index_t *type_indices = malloc(sizeof(type##_index_t) * n);                              \
        size_t i;                                                                                       \
        for (i = 0; i < n; i++) {                                                                       \
            type_indices[i] = (type##_index_t){i, array[i]};                                            \
        }                                                                                               \
        ks_introsort(type##_indices, n, type_indices);                                                  \
        size_t *indices = malloc(sizeof(size_t) * n);                                                   \
        for (i = 0; i < n; i++) {                                                                       \
            indices[i] = type_indices[i].index;                                                         \
        }                                                                                               \
        free(type_indices);                                                                             \
        return indices;                                                                                 \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_add(type *array, type c, size_t n) {                                      \
        for (size_t i = 0; i < n; i++) {                                                                \
            array[i] += c;                                                                              \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_sub(type *array, type c, size_t n) {                                      \
        for (size_t i = 0; i < n; i++) {                                                                \
            array[i] -= c;                                                                              \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_mul(type *array, type c, size_t n) {                                      \
        for (size_t i = 0; i < n; i++) {                                                                \
            array[i] *= c;                                                                              \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_div(type *array, type c, size_t n) {                                      \
        for (size_t i = 0; i < n; i++) {                                                                \
            array[i] /= c;                                                                              \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline type name##_sum(type *array, size_t n) {                                              \
        type result = 0;                                                                                \
        for (size_t i = 0; i < n; i++) {                                                                \
            result += array[i];                                                                         \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline unsigned_type name##_l1_norm(type *array, size_t n) {                                 \
        unsigned_type result = 0;                                                                       \
        for (size_t i = 0; i < n; i++) {                                                                \
            result += type_abs(array[i]);                                                               \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline double name##_l2_norm(type *array, size_t n) {                                        \
        unsigned_type result = 0;                                                                       \
        for (size_t i = 0; i < n; i++) {                                                                \
            result += array[i] * array[i];                                                              \
        }                                                                                               \
        return sqrt((double)result);                                                                    \
    }                                                                                                   \
                                                                                                        \
    static inline unsigned_type name##_sum_sq(type *array, size_t n) {                                  \
        unsigned_type result = 0;                                                                       \
        for (size_t i = 0; i < n; i++) {                                                                \
            result += array[i] * array[i];                                                              \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline double name##_mean(type *array, size_t n) {                                           \
        unsigned_type sum = name##_sum(array, n);                                                       \
        return (double)sum / n;                                                                         \
    }                                                                                                   \
                                                                                                        \
    static inline double name##_var(type *array, size_t n) {                                            \
        double mu = name##_mean(array, n);                                                              \
        double sigma2 = 0.0;                                                                            \
        for (size_t i = 0; i < n; i++) {                                                                \
            double dev = (double)array[i] - mu;                                                         \
            sigma2 += dev * dev;                                                                        \
        }                                                                                               \
        return sigma2 / n;                                                                              \
    }                                                                                                   \
                                                                                                        \
    static inline double name##_std(type *array, size_t n) {                                            \
        double sigma2 = name##_var(array, n);                                                           \
        return sqrt(sigma2);                                                                            \
    }                                                                                                   \
                                                                                                        \
    static inline type name##_product(type *array, size_t n) {                                          \
        type result = 0;                                                                                \
        for (size_t i = 0; i < n; i++) {                                                                \
            result *= array[i];                                                                         \
        }                                                                                               \
        return result;                                                                                  \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_add_array(type *a1, const type *a2, size_t n) {                           \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] += a2[i];                                                                             \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_add_array_times_scalar(type *a1, const type *a2, double v, size_t n) {    \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] += a2[i] * v;                                                                         \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_sub_array(type *a1, const type *a2, size_t n) {                           \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] -= a2[i];                                                                             \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
                                                                                                        \
    static inline void name##_sub_array_times_scalar(type *a1, const type *a2, double v, size_t n) {    \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] -= a2[i] * v;                                                                         \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_mul_array(type *a1, const type *a2, size_t n) {                           \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] *= a2[i];                                                                             \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_mul_array_times_scalar(type *a1, const type *a2, double v, size_t n) {    \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] *= a2[i] * v;                                                                         \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_div_array(type *a1, const type *a2, size_t n) {                           \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] /= a2[i];                                                                             \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline void name##_div_array_times_scalar(type *a1, const type *a2, double v, size_t n) {    \
        for (size_t i = 0; i < n; i++) {                                                                \
            a1[i] /= a2[i] * v;                                                                         \
        }                                                                                               \
    }                                                                                                   \
                                                                                                        \
    static inline type name##_dot(const type *a1, const type *a2, size_t n) {                           \
        type result = 0;                                                                                \
        for (size_t i = 0; i < n; i++) {                                                                \
            result += a1[i] * a2[i];                                                                    \
        }                                                                                               \
        return result;                                                                                  \
    }


#define VECTOR_INIT_NUMERIC_FLOAT(name, type, type_abs)                        \
    VECTOR_INIT_NUMERIC(name, type, type, type_abs)                            \
                                                                               \
    static inline void name##_log(type *array, size_t n) {                     \
        for (size_t i = 0; i < n; i++) {                                       \
            array[i] = log(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_exp(type *array, size_t n) {                     \
        for (size_t i = 0; i < n; i++) {                                       \
            array[i] = exp(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline type name##_sum_log(type *array, size_t n) {                 \
        type result = 0;                                                       \
        for (size_t i = 0; i < n; i++) {                                       \
            result += log(array[i]);                                           \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum_exp(type *array, size_t n) {             \
        type max = name##_max(array, n);                                       \
        type result = 0;                                                       \
        for (size_t i = 0; i < n; i++) {                                       \
            result += exp(array[i] - max);                                     \
        }                                                                      \
        return max + log(result);                                              \
    }



#if defined(INTEL_SSE) || defined(ARM_NEON)
/*
From https://github.com/herumi/fmath/blob/master/fastexp.cpp

The best performing C routine appears to be this version of the Remez algorithm:
Remez 9th [0,log2] SSE
*/
static inline void remez9_0_log2_sse(double *values, size_t num)
{
    size_t i;
    CONST_128D(one, 1.);
    CONST_128D(log2e, 1.4426950408889634073599);
    CONST_128D(maxlog, 7.09782712893383996843e2);   // log(2**1024)
    CONST_128D(minlog, -7.08396418532264106224e2);  // log(2**-1022)
    CONST_128D(c1, 6.93145751953125E-1);
    CONST_128D(c2, 1.42860682030941723212E-6);
    CONST_128D(w9, 3.9099787920346160288874633639268318097077213911751e-6);
    CONST_128D(w8, 2.299608440919942766555719515783308016700833740918e-5);
    CONST_128D(w7, 1.99930498409474044486498978862963995247838069436646e-4);
    CONST_128D(w6, 1.38812674551586429265054343505879910146775323730237e-3);
    CONST_128D(w5, 8.3335688409829575034112982839739473866857586300664e-3);
    CONST_128D(w4, 4.1666622504201078708502686068113075402683415962893e-2);
    CONST_128D(w3, 0.166666671414320541875332123507829990378055646330574);
    CONST_128D(w2, 0.49999999974109940909767965915362308135415179642286);
    CONST_128D(w1, 1.0000000000054730504284163017295863259125942049362);
    CONST_128D(w0, 0.99999999999998091336479463057053516986466888462081);
    const __m128i offset = _mm_setr_epi32(1023, 1023, 0, 0);

    for (i = 0;i < num;i += 4) {
        __m128i k1, k2;
        __m128d p1, p2;
        __m128d a1, a2;
        __m128d xmm0, xmm1;
        __m128d x1, x2;

        /* Load four double values. */
        xmm0 = _mm_load_pd(maxlog);
        xmm1 = _mm_load_pd(minlog);
        x1 = _mm_load_pd(values+i);
        x2 = _mm_load_pd(values+i+2);
        x1 = _mm_min_pd(x1, xmm0);
        x2 = _mm_min_pd(x2, xmm0);
        x1 = _mm_max_pd(x1, xmm1);
        x2 = _mm_max_pd(x2, xmm1);

        /* a = x / log2; */
        xmm0 = _mm_load_pd(log2e);
        xmm1 = _mm_setzero_pd();
        a1 = _mm_mul_pd(x1, xmm0);
        a2 = _mm_mul_pd(x2, xmm0);

        /* k = (int)floor(a); p = (float)k; */
        p1 = _mm_cmplt_pd(a1, xmm1);
        p2 = _mm_cmplt_pd(a2, xmm1);
        xmm0 = _mm_load_pd(one);
        p1 = _mm_and_pd(p1, xmm0);
        p2 = _mm_and_pd(p2, xmm0);
        a1 = _mm_sub_pd(a1, p1);
        a2 = _mm_sub_pd(a2, p2);
        k1 = _mm_cvttpd_epi32(a1);
        k2 = _mm_cvttpd_epi32(a2);
        p1 = _mm_cvtepi32_pd(k1);
        p2 = _mm_cvtepi32_pd(k2);

        /* x -= p * log2; */
        xmm0 = _mm_load_pd(c1);
        xmm1 = _mm_load_pd(c2);
        a1 = _mm_mul_pd(p1, xmm0);
        a2 = _mm_mul_pd(p2, xmm0);
        x1 = _mm_sub_pd(x1, a1);
        x2 = _mm_sub_pd(x2, a2);
        a1 = _mm_mul_pd(p1, xmm1);
        a2 = _mm_mul_pd(p2, xmm1);
        x1 = _mm_sub_pd(x1, a1);
        x2 = _mm_sub_pd(x2, a2);

        /* Compute e^x using a polynomial approximation. */
        xmm0 = _mm_load_pd(w9);
        xmm1 = _mm_load_pd(w8);
        a1 = _mm_mul_pd(x1, xmm0);
        a2 = _mm_mul_pd(x2, xmm0);
        a1 = _mm_add_pd(a1, xmm1);
        a2 = _mm_add_pd(a2, xmm1);

        xmm0 = _mm_load_pd(w7);
        xmm1 = _mm_load_pd(w6);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm0);
        a2 = _mm_add_pd(a2, xmm0);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm1);
        a2 = _mm_add_pd(a2, xmm1);

        xmm0 = _mm_load_pd(w5);
        xmm1 = _mm_load_pd(w4);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm0);
        a2 = _mm_add_pd(a2, xmm0);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm1);
        a2 = _mm_add_pd(a2, xmm1);

        xmm0 = _mm_load_pd(w3);
        xmm1 = _mm_load_pd(w2);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm0);
        a2 = _mm_add_pd(a2, xmm0);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm1);
        a2 = _mm_add_pd(a2, xmm1);

        xmm0 = _mm_load_pd(w1);
        xmm1 = _mm_load_pd(w0);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm0);
        a2 = _mm_add_pd(a2, xmm0);
        a1 = _mm_mul_pd(a1, x1);
        a2 = _mm_mul_pd(a2, x2);
        a1 = _mm_add_pd(a1, xmm1);
        a2 = _mm_add_pd(a2, xmm1);

        /* p = 2^k; */
        k1 = _mm_add_epi32(k1, offset);
        k2 = _mm_add_epi32(k2, offset);
        k1 = _mm_slli_epi32(k1, 20);
        k2 = _mm_slli_epi32(k2, 20);
        k1 = _mm_shuffle_epi32(k1, _MM_SHUFFLE(1,3,0,2));
        k2 = _mm_shuffle_epi32(k2, _MM_SHUFFLE(1,3,0,2));
        p1 = _mm_castsi128_pd(k1);
        p2 = _mm_castsi128_pd(k2);

        /* a *= 2^k. */
        a1 = _mm_mul_pd(a1, p1);
        a2 = _mm_mul_pd(a2, p2);

        /* Store the results. */
        _mm_store_pd(values+i, a1);
        _mm_store_pd(values+i+2, a2);
    }
}


// TODO: look into SIMD log function
#define VECTOR_INIT_NUMERIC_DOUBLE(name, type, type_abs)                       \
    VECTOR_INIT_NUMERIC(name, type, type, type_abs)                            \
                                                                               \
    static inline void name##_log(type *array, size_t n) {                     \
        for (size_t i = 0; i < n; i++) {                                       \
            array[i] = log(array[i]);                                          \
        }                                                                      \
    }                                                                          \
                                                                               \
    static inline void name##_exp(type *array, size_t n) {                     \
        remez9_0_log2_sse(array, n);                                           \
    }                                                                          \
                                                                               \
    static inline type name##_sum_log(type *array, size_t n) {                 \
        type result = 0;                                                       \
        for (size_t i = 0; i < n; i++) {                                       \
            result += log(array[i]);                                           \
        }                                                                      \
        return result;                                                         \
    }                                                                          \
                                                                               \
    static inline type name##_log_sum_exp(type *array, size_t n) {             \
        type max = name##_max(array, n);                                       \
        type result = 0;                                                       \
        for (size_t i = 0; i < n; i++) {                                       \
            result += exp(array[i] - max);                                     \
        }                                                                      \
        return max + log(result);                                              \
    }

#else
#define VECTOR_INIT_NUMERIC_DOUBLE VECTOR_INIT_NUMERIC_FLOAT
#endif



#endif
