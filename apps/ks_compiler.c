/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/decimal.h"
#include "kolibri/digits.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void vyvesti_spravku(void) {
    fprintf(stderr,
            "Использование: ks_compiler [--decode] [-o файл] [вход]\n"
            "  --decode       Преобразовать цифровой поток обратно в текст\n"
            "  -o файл        Путь для сохранения результата (по умолчанию stdout)\n"
            "  вход           Файл KolibriScript (.ks или .ksd), '-' — stdin\n");
}

static unsigned char *chtenie_vseh_bajtov(const char *put, size_t *dlina) {
    FILE *istochnik = NULL;
    if (!put || strcmp(put, "-") == 0) {
        istochnik = stdin;
    } else {
        istochnik = fopen(put, "rb");
        if (!istochnik) {
            fprintf(stderr, "[Ошибка] Не удалось открыть '%s': %s\n", put,
                    strerror(errno));
            return NULL;
        }
    }

    size_t emkost = 4096U;
    size_t dlina_chtenija = 0U;
    unsigned char *dannye = (unsigned char *)malloc(emkost);
    if (!dannye) {
        fprintf(stderr, "[Ошибка] Недостаточно памяти для чтения\n");
        if (istochnik != stdin) {
            fclose(istochnik);
        }
        return NULL;
    }

    while (1) {
        if (dlina_chtenija == emkost) {
            size_t novaja = emkost * 2U;
            unsigned char *novyj =
                (unsigned char *)realloc(dannye, novaja);
            if (!novyj) {
                fprintf(stderr, "[Ошибка] Недостаточно памяти при чтении\n");
                free(dannye);
                if (istochnik != stdin) {
                    fclose(istochnik);
                }
                return NULL;
            }
            dannye = novyj;
            emkost = novaja;
        }
        size_t prochitano = fread(dannye + dlina_chtenija, 1U,
                                  emkost - dlina_chtenija, istochnik);
        dlina_chtenija += prochitano;
        if (prochitano == 0U) {
            if (ferror(istochnik)) {
                fprintf(stderr, "[Ошибка] Ошибка чтения: %s\n",
                        strerror(errno));
                free(dannye);
                if (istochnik != stdin) {
                    fclose(istochnik);
                }
                return NULL;
            }
            break;
        }
    }

    if (istochnik != stdin) {
        fclose(istochnik);
    }

    *dlina = dlina_chtenija;
    return dannye;
}

static int zapisat_vyhod(const char *put, const unsigned char *dannye,
                         size_t dlina) {
    FILE *naznachenie = NULL;
    if (!put || strcmp(put, "-") == 0) {
        naznachenie = stdout;
    } else {
        naznachenie = fopen(put, "wb");
        if (!naznachenie) {
            fprintf(stderr, "[Ошибка] Не удалось открыть '%s' для записи: %s\n",
                    put, strerror(errno));
            return -1;
        }
    }

    size_t zapisano = fwrite(dannye, 1U, dlina, naznachenie);
    if (zapisano != dlina) {
        fprintf(stderr, "[Ошибка] Не удалось полностью записать данные\n");
        if (naznachenie != stdout) {
            fclose(naznachenie);
        }
        return -1;
    }

    if (naznachenie != stdout) {
        fclose(naznachenie);
    }
    return 0;
}

static int dekodirovat(const char *vyhod, const unsigned char *vhod,
                       size_t dlina) {
    uint8_t *cifry = (uint8_t *)malloc(dlina * sizeof(uint8_t));
    if (!cifry) {
        fprintf(stderr, "[Ошибка] Недостаточно памяти для цифрового буфера\n");
        return -1;
    }
    kolibri_potok_cifr potok;
    kolibri_potok_cifr_init(&potok, cifry, dlina);
    for (size_t indeks = 0U; indeks < dlina; ++indeks) {
        unsigned char simvol = vhod[indeks];
        if (simvol >= '0' && simvol <= '9') {
            if (kolibri_potok_cifr_push(&potok, (uint8_t)(simvol - '0')) != 0) {
                free(cifry);
                fprintf(stderr,
                        "[Ошибка] Цифровой поток превышает допустимую длину\n");
                return -1;
            }
        }
    }

    size_t ocenennaja_dlina = kolibri_dlina_dekodirovki_teksta(potok.dlina);
    if (ocenennaja_dlina == 0U) {
        fprintf(stderr, "[Ошибка] Некратное тройке количество цифр\n");
        free(cifry);
        return -1;
    }

    unsigned char *rezultat =
        (unsigned char *)malloc(ocenennaja_dlina * sizeof(unsigned char));
    if (!rezultat) {
        fprintf(stderr, "[Ошибка] Недостаточно памяти для результата\n");
        free(cifry);
        return -1;
    }

    size_t zapisano = 0U;
    int kod = kolibri_izluchit_utf8(&potok, rezultat, ocenennaja_dlina,
                                    &zapisano);
    if (kod != 0) {
        fprintf(stderr, "[Ошибка] Не удалось декодировать цифровой поток\n");
        free(cifry);
        free(rezultat);
        return -1;
    }

    int zapis = zapisat_vyhod(vyhod, rezultat, zapisano);
    free(cifry);
    free(rezultat);
    return zapis;
}

static int kodirovat(const char *vyhod, const unsigned char *vhod,
                     size_t dlina) {
    size_t trebuemye_cifry = kolibri_dlina_kodirovki_teksta(dlina);
    uint8_t *cifry = (uint8_t *)calloc(trebuemye_cifry, sizeof(uint8_t));
    if (!cifry) {
        fprintf(stderr, "[Ошибка] Недостаточно памяти для цифрового потока\n");
        return -1;
    }
    kolibri_potok_cifr potok;
    kolibri_potok_cifr_init(&potok, cifry, trebuemye_cifry);
    if (kolibri_transducirovat_utf8(&potok, vhod, dlina) != 0) {
        fprintf(stderr, "[Ошибка] Не удалось преобразовать текст в цифры\n");
        free(cifry);
        return -1;
    }

    char *stroka = (char *)malloc(potok.dlina + 2U);
    if (!stroka) {
        fprintf(stderr, "[Ошибка] Недостаточно памяти для результата\n");
        free(cifry);
        return -1;
    }
    for (size_t indeks = 0U; indeks < potok.dlina; ++indeks) {
        stroka[indeks] = (char)('0' + potok.danniye[indeks]);
    }
    stroka[potok.dlina] = '\n';
    stroka[potok.dlina + 1U] = '\0';

    int zapis = zapisat_vyhod(vyhod, (const unsigned char *)stroka,
                              potok.dlina + 1U);
    free(stroka);
    free(cifry);
    return zapis;
}

int main(int argc, char **argv) {
    const char *vyhod = NULL;
    const char *vhod = NULL;
    bool decode = false;

    for (int indeks = 1; indeks < argc; ++indeks) {
        if (strcmp(argv[indeks], "--decode") == 0) {
            decode = true;
        } else if (strcmp(argv[indeks], "-o") == 0) {
            if (indeks + 1 >= argc) {
                vyvesti_spravku();
                return 1;
            }
            vyhod = argv[++indeks];
        } else if (strcmp(argv[indeks], "-h") == 0 ||
                   strcmp(argv[indeks], "--help") == 0) {
            vyvesti_spravku();
            return 0;
        } else if (!vhod) {
            vhod = argv[indeks];
        } else {
            fprintf(stderr, "[Ошибка] Лишний аргумент '%s'\n", argv[indeks]);
            vyvesti_spravku();
            return 1;
        }
    }

    size_t dlina = 0U;
    unsigned char *dannye = chtenie_vseh_bajtov(vhod, &dlina);
    if (!dannye) {
        return 1;
    }
    if (decode) {
        int kod = dekodirovat(vyhod, dannye, dlina);
        free(dannye);
        return kod == 0 ? 0 : 1;
    }
    int kod = kodirovat(vyhod, dannye, dlina);
    free(dannye);
    return kod == 0 ? 0 : 1;
}
