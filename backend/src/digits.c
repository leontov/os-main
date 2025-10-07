#include "kolibri/digits.h"

int kolibri_potok_cifr_init(kolibri_potok_cifr *p, uint8_t *buf, size_t cap) {
    if (!p || !buf || cap == 0) return -1;
    p->danniye = buf; p->emkost = cap; p->dlina = 0; p->pozitsiya = 0;
    return 0;
}
void kolibri_potok_cifr_sbros(kolibri_potok_cifr *p)      { if (p){ p->dlina = 0; p->pozitsiya = 0; } }
void kolibri_potok_cifr_vernutsya(kolibri_potok_cifr *p)  { if (p){ p->pozitsiya = 0; } }

int kolibri_potok_cifr_push(kolibri_potok_cifr *p, uint8_t digit) {
    if (!p) return -1;
    if (digit > 9U) return -2;
    if (p->dlina >= p->emkost) return -3;
    p->danniye[p->dlina++] = digit;
    return 0;
}

static inline void split3(uint8_t b, uint8_t *s, uint8_t *d, uint8_t *e) {
    *s = (uint8_t)(b / 100U);
    b  = (uint8_t)(b % 100U);
    *d = (uint8_t)(b / 10U);
    *e = (uint8_t)(b % 10U);
}

int kolibri_transducirovat_utf8(kolibri_potok_cifr *p, const uint8_t *utf8, size_t n) {
    if (!p || !utf8) return -1;
    size_t need = kolibri_dlina_kodirovki_teksta(n);
    if (p->emkost - p->dlina < need) return -2;
    for (size_t i = 0; i < n; ++i) {
        uint8_t s,d,e; split3(utf8[i], &s, &d, &e);
        if (kolibri_potok_cifr_push(p,s)) return -3;
        if (kolibri_potok_cifr_push(p,d)) return -3;
        if (kolibri_potok_cifr_push(p,e)) return -3;
    }
    return 0;
}

int kolibri_izluchit_utf8(const kolibri_potok_cifr *p, uint8_t *out, size_t out_cap, size_t *written) {
    if (written) *written = 0;
    if (!p || !out) return -1;
    if (p->dlina % 3U) return -2;
    size_t need = kolibri_dlina_dekodirovki_teksta(p->dlina);
    if (out_cap < need) return -3;
    size_t pos = 0, w = 0;
    while (pos + 2 < p->dlina) {
        uint8_t s = p->danniye[pos++], d = p->danniye[pos++], e = p->danniye[pos++];
        if (s>9 || d>9 || e>9) return -4;
        uint16_t val = (uint16_t)(s*100U + d*10U + e);
        if (val > 255U) return -5;
        out[w++] = (uint8_t)val;
    }
    if (written) *written = w;
    return 0;
}
