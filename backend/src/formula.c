/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/formula.h"

#include <math.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KOLIBRI_FORMULA_CAPACITY (sizeof(((KolibriFormulaPool *)0)->formulas) / sizeof(KolibriFormula))
#define KOLIBRI_DIGIT_MAX 9U

static uint8_t random_digit(KolibriFormulaPool *pool) {
    return (uint8_t)(k_rng_next(&pool->rng) % 10ULL);
}

static void gene_randomize(KolibriFormulaPool *pool, KolibriGene *gene) {
    gene->length = sizeof(gene->digits);
    for (size_t i = 0; i < gene->length; ++i) {
        gene->digits[i] = random_digit(pool);
    }
}

static int gene_copy(const KolibriGene *src, KolibriGene *dst) {
    if (!src || !dst) {
        return -1;
    }
    if (src->length > sizeof(dst->digits)) {
        return -1;
    }
    dst->length = src->length;
    memcpy(dst->digits, src->digits, src->length);
    return 0;
}

static int decode_signed(const KolibriGene *gene, size_t offset, int *value) {
    if (!gene || !value) {
        return -1;
    }
    if (offset + 3 >= gene->length) {
        return -1;
    }
    int sign = gene->digits[offset] % 2 == 0 ? 1 : -1;
    int magnitude = (int)(gene->digits[offset + 1] * 10 + gene->digits[offset + 2]);
    *value = sign * magnitude;
    return 0;
}

static int decode_operation(const KolibriGene *gene, size_t offset, int *operation) {
    if (!gene || !operation) {
        return -1;
    }
    if (offset >= gene->length) {
        return -1;
    }
    *operation = (int)(gene->digits[offset] % 4U);
    return 0;
}

static int decode_bias(const KolibriGene *gene, size_t offset, int *bias) {
    if (!gene || !bias) {
        return -1;
    }
    if (offset + 2 >= gene->length) {
        return -1;
    }
    int sign = gene->digits[offset] % 2 == 0 ? 1 : -1;
    int magnitude = (int)(gene->digits[offset + 1] * 10 + gene->digits[offset + 2]);
    *bias = sign * magnitude;
    return 0;
}

static int formula_predict(const KolibriFormula *formula, int input, int *output) {
    if (!formula || !output) {
        return -1;
    }
    int operation = 0;
    int slope = 0;
    int bias = 0;
    int auxiliary = 0;
    if (decode_operation(&formula->gene, 0, &operation) != 0 ||
        decode_signed(&formula->gene, 1, &slope) != 0 ||
        decode_bias(&formula->gene, 4, &bias) != 0 ||
        decode_signed(&formula->gene, 7, &auxiliary) != 0) {
        return -1;
    }
    long long result = 0;
    switch (operation) {
    case 0:
        result = (long long)slope * (long long)input + bias;
        break;
    case 1:
        result = (long long)slope * (long long)input - bias;
        break;
    case 2: {
        long long divisor = auxiliary == 0 ? 1 : auxiliary;
        result = ((long long)slope * (long long)input) % divisor;
        result += bias;
        break;
    }
    case 3:
        result = (long long)slope * (long long)input * (long long)input + bias;
        break;
    default:
        result = bias;
        break;
    }
    if (result > 2147483647LL) {
        result = 2147483647LL;
    }
    if (result < -2147483648LL) {
        result = -2147483648LL;
    }
    *output = (int)result;
    return 0;
}

static double complexity_penalty(const KolibriGene *gene) {
    double penalty = 0.0;
    for (size_t i = 0; i < gene->length; ++i) {
        if (gene->digits[i] == 0) {
            continue;
        }
        penalty += 0.001 * (double)(gene->digits[i]);
    }
    return penalty;
}

static double evaluate_formula(const KolibriFormula *formula, const KolibriFormulaPool *pool) {
    if (!formula || !pool || pool->examples == 0) {
        return 0.0;
    }
    double total_error = 0.0;
    for (size_t i = 0; i < pool->examples; ++i) {
        int prediction = 0;
        if (formula_predict(formula, pool->inputs[i], &prediction) != 0) {
            return 0.0;
        }
        int diff = pool->targets[i] - prediction;
        total_error += fabs((double)diff);
    }
    double penalty = complexity_penalty(&formula->gene);
    double fitness = 1.0 / (1.0 + total_error + penalty);
    return fitness;
}

static void apply_feedback_bonus(KolibriFormula *formula, double *fitness) {
    if (!formula || !fitness) {
        return;
    }
    double adjusted = *fitness + formula->feedback;
    if (adjusted < 0.0) {
        adjusted = 0.0;
    }
    if (adjusted > 1.0) {
        adjusted = 1.0;
    }
    *fitness = adjusted;
}

static void mutate_gene(KolibriFormulaPool *pool, KolibriGene *gene) {
    if (!gene) {
        return;
    }
    size_t index = (size_t)(k_rng_next(&pool->rng) % gene->length);
    uint8_t delta = random_digit(pool);
    gene->digits[index] = delta;
}

static void crossover(KolibriFormulaPool *pool, const KolibriGene *parent_a, const KolibriGene *parent_b, KolibriGene *child) {
    (void)pool;
    if (!parent_a || !parent_b || !child) {
        return;
    }
    size_t split = parent_a->length / 2;
    child->length = parent_a->length;
    for (size_t i = 0; i < child->length; ++i) {
        if (i < split) {
            child->digits[i] = parent_a->digits[i];
        } else {
            child->digits[i] = parent_b->digits[i];
        }
    }
}

static int compare_formulas(const void *lhs, const void *rhs) {
    const KolibriFormula *a = (const KolibriFormula *)lhs;
    const KolibriFormula *b = (const KolibriFormula *)rhs;
    if (a->fitness < b->fitness) {
        return 1;
    }
    if (a->fitness > b->fitness) {
        return -1;
    }
    return 0;
}

void kf_pool_init(KolibriFormulaPool *pool, uint64_t seed) {
    if (!pool) {
        return;
    }
    pool->count = KOLIBRI_FORMULA_CAPACITY;
    pool->examples = 0;
    k_rng_seed(&pool->rng, seed);
    for (size_t i = 0; i < pool->count; ++i) {
        gene_randomize(pool, &pool->formulas[i].gene);
        pool->formulas[i].fitness = 0.0;
        pool->formulas[i].feedback = 0.0;
    }
}

void kf_pool_clear_examples(KolibriFormulaPool *pool) {
    if (!pool) {
        return;
    }
    pool->examples = 0;
}

int kf_pool_add_example(KolibriFormulaPool *pool, int input, int target) {
    if (!pool) {
        return -1;
    }
    if (pool->examples >= sizeof(pool->inputs) / sizeof(pool->inputs[0])) {
        return -1;
    }
    pool->inputs[pool->examples] = input;
    pool->targets[pool->examples] = target;
    pool->examples++;
    return 0;
}

static void reproduce(KolibriFormulaPool *pool) {
    size_t elite = pool->count / 3U;
    if (elite == 0) {
        elite = 1;
    }
    for (size_t i = elite; i < pool->count; ++i) {
        size_t parent_a_index = i % elite;
        size_t parent_b_index = (i + 1) % elite;
        KolibriGene child;
        crossover(pool, &pool->formulas[parent_a_index].gene,
                  &pool->formulas[parent_b_index].gene, &child);
        mutate_gene(pool, &child);
        gene_copy(&child, &pool->formulas[i].gene);
        pool->formulas[i].fitness = 0.0;
        pool->formulas[i].feedback = 0.0;
    }
}

void kf_pool_tick(KolibriFormulaPool *pool, size_t generations) {
    if (!pool || pool->count == 0 || pool->examples == 0) {
        return;
    }
    if (generations == 0) {
        generations = 1;
    }
    for (size_t g = 0; g < generations; ++g) {
        for (size_t i = 0; i < pool->count; ++i) {
            double fitness = evaluate_formula(&pool->formulas[i], pool);
            apply_feedback_bonus(&pool->formulas[i], &fitness);
            pool->formulas[i].fitness = fitness;
        }
        qsort(pool->formulas, pool->count, sizeof(KolibriFormula), compare_formulas);
        reproduce(pool);
    }
}

const KolibriFormula *kf_pool_best(const KolibriFormulaPool *pool) {
    if (!pool || pool->count == 0) {
        return NULL;
    }
    return &pool->formulas[0];
}

int kf_formula_apply(const KolibriFormula *formula, int input, int *output) {
    return formula_predict(formula, input, output);
}

size_t kf_formula_digits(const KolibriFormula *formula, uint8_t *out, size_t out_len) {
    if (!formula || !out) {
        return 0;
    }
    if (out_len < formula->gene.length) {
        return 0;
    }
    memcpy(out, formula->gene.digits, formula->gene.length);
    return formula->gene.length;
}

int kf_formula_describe(const KolibriFormula *formula, char *buffer, size_t buffer_len) {
    if (!formula || !buffer || buffer_len == 0) {
        return -1;
    }
    int operation = 0;
    int slope = 0;
    int bias = 0;
    int auxiliary = 0;
    if (decode_operation(&formula->gene, 0, &operation) != 0 ||
        decode_signed(&formula->gene, 1, &slope) != 0 ||
        decode_bias(&formula->gene, 4, &bias) != 0 ||
        decode_signed(&formula->gene, 7, &auxiliary) != 0) {
        return -1;
    }
    const char *operation_name = NULL;
    switch (operation) {
    case 0:
        operation_name = "линейная";
        break;
    case 1:
        operation_name = "инверсная";
        break;
    case 2:
        operation_name = "остаточная";
        break;
    case 3:
        operation_name = "квадратичная";
        break;
    default:
        operation_name = "неизвестная";
        break;
    }
    int written = snprintf(buffer, buffer_len,
                           "тип=%s k=%d b=%d aux=%d фитнес=%.6f",
                           operation_name, slope, bias, auxiliary, formula->fitness);
    if (written < 0 || (size_t)written >= buffer_len) {
        return -1;
    }
    return 0;
}

static void adjust_feedback(KolibriFormula *formula, double delta) {
    if (!formula) {
        return;
    }
    formula->feedback += delta;
    if (formula->feedback > 1.0) {
        formula->feedback = 1.0;
    }
    if (formula->feedback < -1.0) {
        formula->feedback = -1.0;
    }
    formula->fitness += delta;
    if (formula->fitness < 0.0) {
        formula->fitness = 0.0;
    }
}

int kf_pool_feedback(KolibriFormulaPool *pool, const KolibriGene *gene, double delta) {
    if (!pool || !gene || pool->count == 0) {
        return -1;
    }
    for (size_t i = 0; i < pool->count; ++i) {
        if (pool->formulas[i].gene.length != gene->length) {
            continue;
        }
        if (memcmp(pool->formulas[i].gene.digits, gene->digits, gene->length) != 0) {
            continue;
        }
        adjust_feedback(&pool->formulas[i], delta);
        size_t index = i;
        if (delta > 0.0) {
            while (index > 0 && pool->formulas[index].fitness > pool->formulas[index - 1].fitness) {
                KolibriFormula tmp = pool->formulas[index - 1];
                pool->formulas[index - 1] = pool->formulas[index];
                pool->formulas[index] = tmp;
                index--;
            }
        } else if (delta < 0.0) {
            while (index + 1 < pool->count &&
                   pool->formulas[index].fitness < pool->formulas[index + 1].fitness) {
                KolibriFormula tmp = pool->formulas[index + 1];
                pool->formulas[index + 1] = pool->formulas[index];
                pool->formulas[index] = tmp;
                index++;
            }
        }
        return 0;
    }
    return -1;
}
