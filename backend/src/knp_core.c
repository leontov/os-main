
#include "knp_core.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#define KNP_HAVE_NEON 1
#else
#define KNP_HAVE_NEON 0
#endif

static inline double knp_abs(double x){ return x<0?-x:x; }

static inline double knp_clamp01(double x){
    if (x < 1e-16) return 1e-16;
    if (x > (1.0-1e-16)) return 1.0-1e-16;
    return x;
}

static inline uint64_t splitmix64(uint64_t x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline double u64_to_unit(uint64_t x) {
    const uint64_t mant = (x >> 11) | 1ULL;
    const double d = (double)mant / (double)(1ULL<<53);
    return knp_clamp01(d);
}

static uint64_t g_seed_base = 0xD1B54A32D192ED03ULL;

void knp_set_seed_base(uint64_t seed) {
    g_seed_base = seed ? seed : 0xD1B54A32D192ED03ULL;
}

uint64_t knp_get_seed_base(void) {
    return g_seed_base;
}

double knp_chi_u64(uint64_t seed) {
    uint64_t a = splitmix64(seed);
    uint64_t b = splitmix64(a ^ 0xD1B54A32D192ED03ULL);
    double u = u64_to_unit(b);
    double t = 1.0 - knp_abs(2.0*u - 1.0);           // tent map
    double l = 4.0 * t * (1.0 - t);                  // logistic refinement
    double y = 0.5*(t + l);
    return knp_clamp01(y);
}

static inline double cheb_Tk(double z, int k) {
    if (k==0) return 1.0;
    if (k==1) return z;
    double Tkm2 = 1.0, Tkm1 = z, Tk = 0.0;
    for (int i=2;i<=k;i++){
        Tk = 2.0*z*Tkm1 - Tkm2;
        Tkm2 = Tkm1; Tkm1 = Tk;
    }
    return Tk;
}

static inline double add_pair_contribution(const double *theta, int k, double Tk, double s) {
#if KNP_HAVE_NEON
    float64x2_t coeff = vld1q_f64(theta + (size_t)2 * k - 1);
    float64x2_t basis = {Tk, s};
    float64x2_t prod = vmulq_f64(coeff, basis);
    return vaddvq_f64(prod);
#else
    double t1 = theta[2 * k - 1];
    double t2 = theta[2 * k];
    return t1 * Tk + t2 * s;
#endif
}

double knp_phi(double x, const double* theta, size_t n_theta) {
    if (!theta || n_theta==0) return x;
    x = knp_clamp01(x);
    double y = theta[0]*x;
    int kmax = (int)((n_theta-1)/2);
    double z = 2.0*x - 1.0; // for Chebyshev
    for (int k=1;k<=kmax;k++){
        double Tk = cheb_Tk(z, k);
        double s  = sin(M_PI*(double)k * x);
        y += add_pair_contribution(theta, k, Tk, s);
    }
    size_t core_len = 1u + (size_t)kmax * 2u;
    if ((size_t)n_theta > core_len) {
        y += theta[core_len];
    }
    return y;
}

static inline double q_to_unit(uint64_t q){
    return u64_to_unit(splitmix64(q));
}

double knp_score_uint64(uint64_t q, double v, const double* theta, size_t n_theta){
    (void)theta; (void)n_theta;
    double qn = q_to_unit(q);
    double vn = knp_clamp01(v);
    return -knp_abs(vn - qn);
}

typedef struct { uint64_t id; double v; double s; } Node;

static void partial_sort_desc(Node* arr, int n){
    for (int i=0;i<n;i++){
        int best=i;
        for (int j=i+1;j<n;j++) if (arr[j].s > arr[best].s) best=j;
        if (best!=i){ Node t=arr[i]; arr[i]=arr[best]; arr[best]=t; }
    }
}

int knp_infer(uint64_t q,
              const double* theta, size_t n_theta,
              int beam, int depth,
              uint64_t* out_best_id, double* out_value, double* out_score)
{
    if (beam<1) beam=1;
    if (depth<1) depth=1;
    if (beam>256) beam=256;

    Node* cur = (Node*)malloc(sizeof(Node)*(size_t)beam);
    Node* nxt = (Node*)malloc(sizeof(Node)*(size_t)beam);
    if (!cur || !nxt){ free(cur); free(nxt); return -1; }

    int cur_count=0;
    for (int d=0; d<10 && cur_count<beam; ++d){
        uint64_t id = splitmix64(g_seed_base ^ q ^ (uint64_t)d);
        double x = knp_chi_u64(id);
        double v = knp_phi(x, theta, n_theta);
        double s = knp_score_uint64(q, v, theta, n_theta);
        cur[cur_count++] = (Node){ id, v, s };
    }
    partial_sort_desc(cur, cur_count);

    for (int level=1; level<depth; ++level){
        int gen=0;
        for (int i=0;i<cur_count;i++){
            uint64_t base = splitmix64(cur[i].id ^ g_seed_base ^ ((uint64_t)level * 0x9E37ULL));
            for (int d=0; d<10 && gen<beam; ++d){
                uint64_t id = splitmix64(base ^ (uint64_t)d);
                double x = knp_chi_u64(id);
                double v = knp_phi(x, theta, n_theta);
                double s = knp_score_uint64(q, v, theta, n_theta);
                nxt[gen++] = (Node){ id, v, s };
            }
            if (gen>=beam) break;
        }
        if (gen==0) break;
        partial_sort_desc(nxt, gen);
        if (gen>beam) gen=beam;
        for (int i=0;i<gen;i++) cur[i]=nxt[i];
        cur_count = gen;
    }

    Node best = cur[0];
    for (int i=1;i<cur_count;i++) if (cur[i].s>best.s) best=cur[i];

    if (out_best_id) *out_best_id = best.id;
    if (out_value) *out_value = best.v;
    if (out_score) *out_score = best.s;

    free(cur); free(nxt);
    return 0;
}
