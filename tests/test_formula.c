#include "kolibri/formula.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void teach_linear_task(KolibriFormulaPool *pool) {
  for (int i = 0; i < 4; ++i) {
    int input = i;
    int target = 2 * i + 1;
    assert(kf_pool_add_example(pool, input, target) == 0);
  }
}

static void assert_deterministic(void) {
  KolibriFormulaPool first;
  KolibriFormulaPool second;
  kf_pool_init(&first, 2025);
  kf_pool_init(&second, 2025);
  teach_linear_task(&first);
  teach_linear_task(&second);
  kf_pool_tick(&first, 64);
  kf_pool_tick(&second, 64);
  const KolibriFormula *best_first = kf_pool_best(&first);
  const KolibriFormula *best_second = kf_pool_best(&second);
  uint8_t digits_first[32];
  uint8_t digits_second[32];
  size_t len_first =
      kf_formula_digits(best_first, digits_first, sizeof(digits_first));
  size_t len_second =
      kf_formula_digits(best_second, digits_second, sizeof(digits_second));
  assert(len_first == len_second);
  assert(memcmp(digits_first, digits_second, len_first) == 0);
}

static void test_feedback_adjustment(void) {
  KolibriFormulaPool pool;
  kf_pool_init(&pool, 321);
  teach_linear_task(&pool);
  kf_pool_tick(&pool, 64);
  const KolibriFormula *best = kf_pool_best(&pool);
  assert(best != NULL);
  KolibriGene snapshot = best->gene;
  double baseline = best->fitness;
  assert(kf_pool_feedback(&pool, &snapshot, 0.3) == 0);
  const KolibriFormula *after_reward = kf_pool_best(&pool);
  assert(after_reward != NULL);
  assert(after_reward->fitness >= baseline);
  assert(kf_pool_feedback(&pool, &snapshot, -0.8) == 0);
  const KolibriFormula *after_penalty = kf_pool_best(&pool);
  assert(after_penalty != NULL);
  assert(after_penalty->fitness >= 0.0);
}

void test_formula(void) {
  KolibriFormulaPool pool;
  kf_pool_init(&pool, 77);
  teach_linear_task(&pool);
  const KolibriFormula *initial = kf_pool_best(&pool);
  int baseline_errors = 0;
  for (int i = 0; i < 4; ++i) {
    int local = 0;
    assert(kf_formula_apply(initial, i, &local) == 0);
    baseline_errors += abs((2 * i + 1) - local);
  }
  kf_pool_tick(&pool, 128);
  const KolibriFormula *best = kf_pool_best(&pool);
  assert(best != NULL);
  int prediction = 0;
  assert(kf_formula_apply(best, 4, &prediction) == 0);
  int errors = 0;
  for (int i = 0; i < 4; ++i) {
    int local = 0;
    assert(kf_formula_apply(best, i, &local) == 0);
    errors += abs((2 * i + 1) - local);
  }
  assert(errors <= baseline_errors);
  assert_deterministic();
  test_feedback_adjustment();
}
