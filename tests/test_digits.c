#include "kolibri/digits.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_digits(void) {
    uint8_t buf[1024]; kolibri_potok_cifr p;
    assert(kolibri_potok_cifr_init(&p, buf, sizeof(buf)) == 0);
    const char *msg = "Privet, Kolibri!";
    size_t n = strlen(msg);
    assert(kolibri_transducirovat_utf8(&p, (const uint8_t *)msg, n) == 0);
    assert(p.dlina == kolibri_dlina_kodirovki_teksta(n));
    uint8_t out[256]; size_t w = 0;
    assert(kolibri_izluchit_utf8(&p, out, sizeof(out), &w) == 0);
    assert(w == n && memcmp(out, msg, n) == 0);
    puts("digits ok");
}
