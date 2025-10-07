#ifndef KOLIBRI_KERNEL_FORMULA_H
#define KOLIBRI_KERNEL_FORMULA_H

#include "kolibri/random.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t digits[32];
    size_t length;
} KolibriGene;

typedef struct {
    KolibriGene gene;
    double fitness;
    double feedback;
} KolibriFormula;

typedef struct {
    KolibriFormula formulas[16];
    size_t count;
    KolibriRng rng;
    int inputs[32];
    int targets[32];
    size_t examples;
} KolibriFormulaPool;

void kf_pool_init(KolibriFormulaPool *pool, uint64_t seed);
void kf_pool_clear_examples(KolibriFormulaPool *pool);
int kf_pool_add_example(KolibriFormulaPool *pool, int input, int target);
void kf_pool_tick(KolibriFormulaPool *pool, size_t generations);
const KolibriFormula *kf_pool_best(const KolibriFormulaPool *pool);
int kf_formula_apply(const KolibriFormula *formula, int input, int *output);
size_t kf_formula_digits(const KolibriFormula *formula, uint8_t *out, size_t out_len);
int kf_formula_describe(const KolibriFormula *formula, char *buffer, size_t buffer_len);
int kf_pool_feedback(KolibriFormulaPool *pool, const KolibriGene *gene, double delta);

#endif /* KOLIBRI_KERNEL_FORMULA_H */
