#include "knp_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FUZZ_ITERATIONS 100000

static uint64_t splitmix64_step(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double to_unit_interval(uint64_t value) {
    return (double)(value % 1000000ULL) / 500000.0 - 1.0;
}

static void fill_theta(uint64_t *state, double *theta, size_t n_theta) {
    for (size_t i = 0; i < n_theta; ++i) {
        uint64_t next = splitmix64_step(state);
        theta[i] = to_unit_interval(next);
    }
    if (n_theta == 0) {
        theta[0] = 1.0;
    }
}

void test_knp_core_fuzz(void) {
    uint64_t state = 0x1234ULL;
    for (int iter = 0; iter < FUZZ_ITERATIONS; ++iter) {
        uint64_t q = splitmix64_step(&state);
        size_t n_theta = (splitmix64_step(&state) % KNP_THETA_MAX) + 1ULL;
        double theta[KNP_THETA_MAX];
        fill_theta(&state, theta, n_theta);
        int beam = (int)(splitmix64_step(&state) % 64ULL) + 1;
        int depth = (int)(splitmix64_step(&state) % 32ULL) + 1;

        uint64_t best_id = 0;
        double value = 0.0;
        double score = 0.0;
        int rc = knp_infer(q, theta, n_theta, beam, depth, &best_id, &value, &score);
        if (rc != 0) {
            fprintf(stderr, "knp_infer failed at iteration %d\n", iter);
            abort();
        }
        if (!isfinite(value) || !isfinite(score)) {
            fprintf(stderr, "non finite result at iteration %d\n", iter);
            abort();
        }
        if (score > 1e-12 || score < -1.0000000001) {
            fprintf(stderr, "score out of range at iteration %d: %f\n", iter, score);
            abort();
        }
        (void)best_id;
    }
}

void test_knp_core_determinism(void) {
    static const uint64_t queries[] = {
        1ULL, 42ULL, 1024ULL, 987654321ULL, 0xABCDEF1234567890ULL
    };
    static const double theta[] = {1.0, 0.3, -0.2, 0.12};
    static const uint64_t expected_ids[] = {
        3767888089082314298ULL,
        10280218862203157818ULL,
        17243144174419232088ULL,
        8904906705999669650ULL,
        1176787856515244532ULL
    };
    static const double expected_values[] = {
        0.56770415738690327,
        0.75019613087727188,
        0.25666110647037216,
        0.77313799722756982,
        0.94301827072673905
    };
    static const double expected_scores[] = {
        -0.0011425822146223785,
        -0.0086312521054485725,
        -0.009554022894547709,
        -0.082245598403004272,
        -0.013291102242261932
    };

    knp_set_seed_base(0xC0FFEEULL);
    for (size_t i = 0; i < sizeof(queries)/sizeof(queries[0]); ++i) {
        uint64_t best_id = 0;
        double value = 0.0;
        double score = 0.0;
        int rc = knp_infer(queries[i], theta, sizeof(theta)/sizeof(theta[0]), 16, 8,
                           &best_id, &value, &score);
        if (rc != 0) {
            fprintf(stderr, "determinism test failed for index %zu\n", i);
            abort();
        }
        if (best_id != expected_ids[i]) {
            fprintf(stderr, "best_id mismatch [%zu]: got %llu expected %llu\n",
                    i, (unsigned long long)best_id, (unsigned long long)expected_ids[i]);
            abort();
        }
        if (fabs(value - expected_values[i]) > 1e-12) {
            fprintf(stderr, "value mismatch [%zu]: got %.17g expected %.17g\n",
                    i, value, expected_values[i]);
            abort();
        }
        if (fabs(score - expected_scores[i]) > 1e-12) {
            fprintf(stderr, "score mismatch [%zu]: got %.17g expected %.17g\n",
                    i, score, expected_scores[i]);
            abort();
        }
    }
    uint64_t digest = 0xDEADBEEFCAFEBABEULL;
    for (uint64_t q = 0; q < 10000ULL; ++q) {
        uint64_t best_id = 0;
        double value = 0.0;
        double score = 0.0;
        knp_infer(q, theta, sizeof(theta)/sizeof(theta[0]), 12, 6,
                  &best_id, &value, &score);
        union {
            double d;
            uint64_t u;
        } conv;
        conv.d = value;
        digest ^= splitmix64_step(&digest);
        digest ^= best_id;
        digest ^= conv.u;
        conv.d = score;
        digest ^= conv.u;
    }
    if (digest != 0x1CB9C6FBFEF43B53ULL) {
        fprintf(stderr, "deterministic digest mismatch: got %llx expected %llx\n",
                (unsigned long long)digest, (unsigned long long)0x1CB9C6FBFEF43B53ULL);
        abort();
    }
    knp_set_seed_base(0xD1B54A32D192ED03ULL);
}
