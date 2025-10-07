#include "kolibri/formula.h"
#include "kolibri/roy.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

static const unsigned char TEST_KEY[] = "kolibri-test-key";

static void zapolnit_formulu(KolibriFormula *formula) {
    formula->gene.length = 3U;
    formula->gene.digits[0] = 1U;
    formula->gene.digits[1] = 2U;
    formula->gene.digits[2] = 3U;
    formula->fitness = 0.75;
    formula->feedback = 0.0;
}

void test_roy(void) {
    KolibriRoy pervyj;
    KolibriRoy vtoroj;
    assert(kolibri_roy_zapustit(&pervyj, 1001U, 51200U, TEST_KEY,
                                sizeof(TEST_KEY) - 1U) == 0);
    assert(kolibri_roy_zapustit(&vtoroj, 2002U, 51201U, TEST_KEY,
                                sizeof(TEST_KEY) - 1U) == 0);

    struct sockaddr_in adres;
    memset(&adres, 0, sizeof(adres));
    adres.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &adres.sin_addr);
    adres.sin_port = htons(51201U);
    assert(kolibri_roy_dobavit_soseda(&pervyj, &adres, 2002U) == 0);
    adres.sin_port = htons(51200U);
    assert(kolibri_roy_dobavit_soseda(&vtoroj, &adres, 1001U) == 0);

    usleep(200000);

    KolibriRoySosed perechen[4];
    size_t chislo = kolibri_roy_spisok_sosedey(
        &pervyj, perechen, sizeof(perechen) / sizeof(perechen[0]));
    assert(chislo > 0U);

    KolibriFormula formula;
    zapolnit_formulu(&formula);
    assert(kolibri_roy_otpravit_vsem(&pervyj, &formula) == 0);

    usleep(200000);

    int nashli_formulu = 0;
    KolibriRoySobytie sobytie;
    while (kolibri_roy_poluchit_sobytie(&vtoroj, &sobytie) > 0) {
        if (sobytie.tip == KOLIBRI_ROY_SOBYTIE_FORMULA) {
            assert(sobytie.formula.gene.length == 3U);
            assert(sobytie.formula.gene.digits[0] == 1U);
            assert(sobytie.formula.gene.digits[1] == 2U);
            assert(sobytie.formula.gene.digits[2] == 3U);
            nashli_formulu = 1;
        }
    }
    assert(nashli_formulu);

    kolibri_roy_ostanovit(&pervyj);
    kolibri_roy_ostanovit(&vtoroj);
}
