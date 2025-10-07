#include <stdio.h>

void test_decimal(void);
void test_genome(void);
void test_formula(void);
void test_net(void);
void test_digits(void);
void test_knp_core_fuzz(void);
void test_knp_core_determinism(void);

int main(void) {
  test_decimal();
  test_genome();
  test_formula();
  test_digits();
  test_net();
  test_knp_core_fuzz();
  test_knp_core_determinism();
  printf("all tests passed\n");
  return 0;
}
