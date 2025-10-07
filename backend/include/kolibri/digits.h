#ifndef KOLIBRI_DIGITS_H
#define KOLIBRI_DIGITS_H

#include <stddef.h>
#include <stdint.h>

/* Поток десятичных цифр 0..9 */
typedef struct {
    uint8_t *danniye;   /* буфер цифр */
    size_t   emkost;    /* вместимость в цифрах */
    size_t   dlina;     /* текущее число цифр */
    size_t   pozitsiya; /* позиция чтения */
} kolibri_potok_cifr;

/* Базовые операции над потоком */
int  kolibri_potok_cifr_init(kolibri_potok_cifr *p, uint8_t *buf, size_t cap);
void kolibri_potok_cifr_sbros(kolibri_potok_cifr *p);
void kolibri_potok_cifr_vernutsya(kolibri_potok_cifr *p);
int  kolibri_potok_cifr_push(kolibri_potok_cifr *p, uint8_t digit);

/* Оценка длин (по 3 цифры на один байт) */
static inline size_t kolibri_dlina_kodirovki_teksta(size_t utf8_len) { return utf8_len * 3U; }
static inline size_t kolibri_dlina_dekodirovki_teksta(size_t digits_len) { return digits_len / 3U; }

/* UTF-8 -> цифры и обратно */
int kolibri_transducirovat_utf8(kolibri_potok_cifr *p,
                                const uint8_t *utf8, size_t n);
int kolibri_izluchit_utf8(const kolibri_potok_cifr *p,
                          uint8_t *out, size_t out_cap, size_t *written);

#endif /* KOLIBRI_DIGITS_H */
