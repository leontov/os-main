
#ifndef KNP_CORE_H
#define KNP_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KNP_THETA_MAX 32

double knp_chi_u64(uint64_t seed);
double knp_phi(double x, const double* theta, size_t n_theta);
double knp_score_uint64(uint64_t q, double v, const double* theta, size_t n_theta);

int knp_infer(uint64_t q,
              const double* theta, size_t n_theta,
              int beam, int depth,
              uint64_t* out_best_id, double* out_value, double* out_score);

void knp_set_seed_base(uint64_t seed);
uint64_t knp_get_seed_base(void);

#ifdef __cplusplus
}
#endif

#endif
