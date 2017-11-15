#include "regularization.h"
#include "float_utils.h"
#include "log/log.h"

inline void regularize_l2(double *theta, size_t n, double reg_update) {
    for (size_t i = 0; i < n; i++) {
        double current_value = theta[i];
        double updated_value = current_value - current_value * reg_update;
        // Make sure the regularization update doesn't change the sign of the weight
        // Otherwise, set the weight to 0
        if ((updated_value > 0) == (current_value > 0)) {
            theta[i] = updated_value;
        } else {
            theta[i] = 0.0;
        }
    }
}

inline void regularize_l1(double *theta, size_t n, double reg_update) {
    for (size_t i = 0; i < n; i++) {
        double current_value = theta[i];
        double updated_value = current_value - sign(current_value) * reg_update;
        // Make sure the regularization update doesn't change the sign of the weight
        // Otherwise, set the weight to 0
        if ((updated_value > 0) == (current_value > 0)) {
            theta[i] = updated_value;
        } else {
            theta[i] = 0.0;
        }
    }
}