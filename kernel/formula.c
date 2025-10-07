#include "kolibri/formula.h"

#include "support.h"

#define KOLIBRI_FORMULA_CAPACITY (sizeof(((KolibriFormulaPool *)0)->formulas) / sizeof(KolibriFormula))

static uint8_t random_digit(KolibriFormulaPool *pool) {
    uint32_t value = (uint32_t)k_rng_next(&pool->rng);
    return (uint8_t)(value % 10U);
}

static void gene_randomize(KolibriFormulaPool *pool, KolibriGene *gene) {
    gene->length = sizeof(gene->digits);
    for (size_t i = 0; i < gene->length; ++i) {
        gene->digits[i] = random_digit(pool);
    }
}

static int decode_coefficients(const KolibriGene *gene, int *slope, int *bias) {
    if (!gene || !slope || !bias || gene->length < 4U) {
        return -1;
    }
    int raw_slope = (int)(gene->digits[0] * 10 + gene->digits[1]);
    int raw_bias = (int)(gene->digits[2] * 10 + gene->digits[3]);
    *slope = (gene->digits[4] % 2U == 0U) ? raw_slope : -raw_slope;
    *bias = (gene->digits[5] % 2U == 0U) ? raw_bias : -raw_bias;
    return 0;
}

static double evaluate_formula(const KolibriFormula *formula, const KolibriFormulaPool *pool) {
    if (!formula || !pool || pool->examples == 0U) {
        return 0.0;
    }
    int slope = 0;
    int bias = 0;
    if (decode_coefficients(&formula->gene, &slope, &bias) != 0) {
        return 0.0;
    }
    double total_error = 0.0;
    for (size_t i = 0; i < pool->examples; ++i) {
        int prediction = slope * pool->inputs[i] + bias;
        int diff = prediction - pool->targets[i];
        if (diff < 0) {
            diff = -diff;
        }
        total_error += (double)diff;
    }
    double normalized = 1.0 / (1.0 + total_error);
    double adjusted = normalized + formula->feedback;
    if (adjusted < 0.0) {
        adjusted = 0.0;
    }
    if (adjusted > 1.0) {
        adjusted = 1.0;
    }
    return adjusted;
}

static void mutate_gene(KolibriFormulaPool *pool, KolibriGene *gene) {
    if (!gene) {
        return;
    }
    uint32_t value = (uint32_t)k_rng_next(&pool->rng);
    size_t index = (size_t)(value % (gene->length ? gene->length : 1U));
    gene->digits[index] = random_digit(pool);
}

void kf_pool_init(KolibriFormulaPool *pool, uint64_t seed) {
    if (!pool) {
        return;
    }
    k_memset(pool, 0, sizeof(*pool));
    k_rng_seed(&pool->rng, seed);
    pool->count = KOLIBRI_FORMULA_CAPACITY;
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
    pool->examples = 0U;
}

int kf_pool_add_example(KolibriFormulaPool *pool, int input, int target) {
    if (!pool || pool->examples >= sizeof(pool->inputs) / sizeof(pool->inputs[0])) {
        return -1;
    }
    pool->inputs[pool->examples] = input;
    pool->targets[pool->examples] = target;
    ++pool->examples;
    return 0;
}

static void evaluate_pool(KolibriFormulaPool *pool) {
    if (!pool) {
        return;
    }
    for (size_t i = 0; i < pool->count; ++i) {
        pool->formulas[i].fitness = evaluate_formula(&pool->formulas[i], pool);
    }
}

static KolibriFormula *select_best(KolibriFormulaPool *pool) {
    if (!pool || pool->count == 0U) {
        return NULL;
    }
    size_t best_index = 0U;
    double best_score = pool->formulas[0].fitness;
    for (size_t i = 1; i < pool->count; ++i) {
        if (pool->formulas[i].fitness > best_score) {
            best_score = pool->formulas[i].fitness;
            best_index = i;
        }
    }
    return &pool->formulas[best_index];
}

void kf_pool_tick(KolibriFormulaPool *pool, size_t generations) {
    if (!pool || pool->count == 0U) {
        return;
    }
    for (size_t gen = 0; gen < generations; ++gen) {
        evaluate_pool(pool);
        KolibriFormula *best = select_best(pool);
        for (size_t i = 0; i < pool->count; ++i) {
            if (&pool->formulas[i] == best) {
                continue;
            }
            if (pool->formulas[i].fitness < 0.5) {
                k_memcpy(&pool->formulas[i].gene, &best->gene, sizeof(KolibriGene));
                mutate_gene(pool, &pool->formulas[i].gene);
            }
        }
    }
    evaluate_pool(pool);
}

const KolibriFormula *kf_pool_best(const KolibriFormulaPool *pool) {
    if (!pool || pool->count == 0U) {
        return NULL;
    }
    const KolibriFormula *best = &pool->formulas[0];
    for (size_t i = 1; i < pool->count; ++i) {
        if (pool->formulas[i].fitness > best->fitness) {
            best = &pool->formulas[i];
        }
    }
    return best;
}

int kf_formula_apply(const KolibriFormula *formula, int input, int *output) {
    if (!formula || !output) {
        return -1;
    }
    int slope = 0;
    int bias = 0;
    if (decode_coefficients(&formula->gene, &slope, &bias) != 0) {
        return -1;
    }
    long long result = (long long)slope * (long long)input + (long long)bias;
    if (result > 2147483647LL) {
        result = 2147483647LL;
    }
    if (result < -2147483648LL) {
        result = -2147483648LL;
    }
    *output = (int)result;
    return 0;
}

size_t kf_formula_digits(const KolibriFormula *formula, uint8_t *out, size_t out_len) {
    if (!formula || !out || out_len == 0U) {
        return 0U;
    }
    size_t count = formula->gene.length;
    if (count > out_len) {
        count = out_len;
    }
    k_memcpy(out, formula->gene.digits, count);
    return count;
}

int kf_formula_describe(const KolibriFormula *formula, char *buffer, size_t buffer_len) {
    if (!formula || !buffer || buffer_len == 0U) {
        return -1;
    }
    int slope = 0;
    int bias = 0;
    if (decode_coefficients(&formula->gene, &slope, &bias) != 0) {
        return -1;
    }
    char sign = bias >= 0 ? '+' : '-';
    int abs_bias = bias >= 0 ? bias : -bias;
    char temp[32];
    size_t len = 0U;
    temp[len++] = 'y';
    temp[len++] = '=';
    if (slope < 0) {
        temp[len++] = '-';
        slope = -slope;
    }
    temp[len++] = (char)('0' + (slope / 10) % 10);
    temp[len++] = (char)('0' + slope % 10);
    temp[len++] = '*';
    temp[len++] = 'x';
    temp[len++] = sign;
    temp[len++] = (char)('0' + (abs_bias / 10) % 10);
    temp[len++] = (char)('0' + abs_bias % 10);
    temp[len] = '\0';
    k_strlcpy(buffer, temp, buffer_len);
    return 0;
}

int kf_pool_feedback(KolibriFormulaPool *pool, const KolibriGene *gene, double delta) {
    if (!pool || !gene) {
        return -1;
    }
    for (size_t i = 0; i < pool->count; ++i) {
        if (pool->formulas[i].gene.length == gene->length &&
            k_memcmp(pool->formulas[i].gene.digits, gene->digits, gene->length) == 0) {
            pool->formulas[i].feedback += delta;
            if (pool->formulas[i].feedback > 1.0) {
                pool->formulas[i].feedback = 1.0;
            }
            if (pool->formulas[i].feedback < -1.0) {
                pool->formulas[i].feedback = -1.0;
            }
            return 0;
        }
    }
    return -1;
}
