/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/script.h"

#include "kolibri/decimal.h"
#include "kolibri/formula.h"
#include "kolibri/genome.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KOLIBRI_SCRIPT_CAPACITY 16384U

/* Сбрасывает внутренний поток цифр и очищает буфер. */
static void skript_sbros(KolibriScript *skript) {
    if (!skript) {
        return;
    }
    kolibri_potok_cifr_sbros(&skript->potok);
}

/* Загружает цифровой поток (.ksd), преобразуя символы '0'..'9' в импульсы. */
static int skript_zagruzit_cifry(KolibriScript *skript, const char *dannye) {
    if (!skript || !dannye) {
        return -1;
    }
    skript_sbros(skript);
    size_t kolichestvo_cifr = 0U;
    for (const unsigned char *ptr = (const unsigned char *)dannye; *ptr; ++ptr) {
        if (*ptr >= '0' && *ptr <= '9') {
            ++kolichestvo_cifr;
        }
    }
    if (kolichestvo_cifr == 0U || kolichestvo_cifr > skript->emkost) {
        return -1;
    }
    kolibri_potok_cifr_sbros(&skript->potok);
    for (const unsigned char *ptr = (const unsigned char *)dannye; *ptr; ++ptr) {
        if (*ptr >= '0' && *ptr <= '9') {
            if (kolibri_potok_cifr_push(&skript->potok,
                                        (uint8_t)(*ptr - (unsigned char)'0')) != 0) {
                return -1;
            }
        }
    }
    kolibri_potok_cifr_vernutsya(&skript->potok);
    return 0;
}

int ks_init(KolibriScript *skript, KolibriFormulaPool *pool,
            KolibriGenome *genome) {
    if (!skript || !pool) {
        return -1;
    }
    memset(skript, 0, sizeof(*skript));
    skript->hranilishe = (uint8_t *)calloc(KOLIBRI_SCRIPT_CAPACITY, sizeof(uint8_t));
    if (!skript->hranilishe) {
        return -1;
    }
    skript->emkost = KOLIBRI_SCRIPT_CAPACITY;
    kolibri_potok_cifr_init(&skript->potok, skript->hranilishe, skript->emkost);
    skript->pool = pool;
    skript->genome = genome;
    skript->vyvod = stdout;
    return 0;
}

void ks_free(KolibriScript *skript) {
    if (!skript) {
        return;
    }
    free(skript->hranilishe);
    skript->hranilishe = NULL;
    skript->emkost = 0U;
    skript->pool = NULL;
    skript->genome = NULL;
    skript->vyvod = NULL;
}

void ks_set_output(KolibriScript *skript, FILE *vyvod) {
    if (!skript) {
        return;
    }
    skript->vyvod = vyvod ? vyvod : stdout;
}

static int ks_zapisat_sobytiye(KolibriScript *skript, const char *event,
                               const char *payload) {
    if (!skript || !event || !skript->genome) {
        return 0;
    }
    return kg_append(skript->genome, event, payload ? payload : "", NULL);
}

int ks_load_text(KolibriScript *skript, const char *text) {
    if (!skript || !text) {
        return -1;
    }
    skript_sbros(skript);
    size_t dlina = strlen(text);
    size_t maksimum = kolibri_dlina_kodirovki_teksta(dlina);
    if (maksimum > skript->emkost) {
        return -1;
    }
    if (kolibri_transducirovat_utf8(&skript->potok,
                                    (const unsigned char *)text, dlina) != 0) {
        return -1;
    }
    kolibri_potok_cifr_vernutsya(&skript->potok);
    return 0;
}

int ks_load_file(KolibriScript *skript, const char *path) {
    if (!skript || !path) {
        return -1;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    char *buffer = (char *)calloc((size_t)size + 1U, sizeof(char));
    if (!buffer) {
        fclose(file);
        return -1;
    }
    size_t read = fread(buffer, 1U, (size_t)size, file);
    fclose(file);
    buffer[read] = '\0';
    bool tolko_cifry = false;
    bool soderzhit_netcifry = false;
    for (size_t indeks = 0U; indeks < read; ++indeks) {
        unsigned char simvol = (unsigned char)buffer[indeks];
        if (simvol >= '0' && simvol <= '9') {
            tolko_cifry = true;
        } else if (!isspace(simvol)) {
            soderzhit_netcifry = true;
            break;
        }
    }
    int status = -1;
    if (tolko_cifry && !soderzhit_netcifry) {
        status = skript_zagruzit_cifry(skript, buffer);
    } else {
        status = ks_load_text(skript, buffer);
    }
    free(buffer);
    return status;
}

/* Удаляет начальные и конечные пробелы, модифицируя строку на месте. */
static void ubrat_probel(char *stroka) {
    if (!stroka) {
        return;
    }
    size_t dlina = strlen(stroka);
    size_t start = 0U;
    while (start < dlina &&
           (unsigned char)stroka[start] <= (unsigned char)' ')
        start++;
    size_t konec = dlina;
    while (konec > start &&
           (unsigned char)stroka[konec - 1U] <= (unsigned char)' ')
        konec--;
    if (start > 0U) {
        memmove(stroka, stroka + start, konec - start);
    }
    stroka[konec - start] = '\0';
}

static int obrabotat_pokazat(KolibriScript *skript, const char *stroka) {
    const char *nachalo = strchr(stroka, '"');
    if (!nachalo) {
        return -1;
    }
    const char *konec = strrchr(stroka, '"');
    if (!konec || konec <= nachalo) {
        return -1;
    }
    size_t dlina = (size_t)(konec - nachalo - 1);
    if (!skript->vyvod) {
        skript->vyvod = stdout;
    }
    fwrite(nachalo + 1, 1U, dlina, skript->vyvod);
    fputc('\n', skript->vyvod);
    return 0;
}

static int obrabotat_obuchit(KolibriScript *skript, char *stroka) {
    char *chislo = strstr(stroka, "число");
    if (!chislo) {
        return -1;
    }
    chislo += strlen("число");
    ubrat_probel(chislo);
    char *strela = strstr(chislo, "->");
    if (!strela) {
        return -1;
    }
    *strela = '\0';
    char *pravaya = strela + 2;
    ubrat_probel(chislo);
    ubrat_probel(pravaya);
    long lev = strtol(chislo, NULL, 10);
    long prav = strtol(pravaya, NULL, 10);
    if (lev < -2147483648L || lev > 2147483647L ||
        prav < -2147483648L || prav > 2147483647L) {
        return -1;
    }
    if (kf_pool_add_example(skript->pool, (int)lev, (int)prav) != 0) {
        return -1;
    }
    ks_zapisat_sobytiye(skript, "SCRIPT_TEACH", "пример добавлен");
    return 0;
}

static int obrabotat_tik(KolibriScript *skript, const char *stroka) {
    const char *chislo = stroka + strlen("тикнуть");
    while (*chislo && (unsigned char)*chislo <= (unsigned char)' ') {
        ++chislo;
    }
    long pokolenija = strtol(chislo, NULL, 10);
    if (pokolenija <= 0L) {
        pokolenija = 1L;
    }
    kf_pool_tick(skript->pool, (size_t)pokolenija);
    ks_zapisat_sobytiye(skript, "SCRIPT_TICK", "эволюция выполнена");
    return 0;
}

static int obrabotat_spros(KolibriScript *skript, const char *stroka) {
    const char *chislo = strstr(stroka, "число");
    if (!chislo) {
        return -1;
    }
    chislo += strlen("число");
    while (*chislo && (unsigned char)*chislo <= (unsigned char)' ') {
        ++chislo;
    }
    long znachenie = strtol(chislo, NULL, 10);
    const KolibriFormula *luchshaja = kf_pool_best(skript->pool);
    if (!luchshaja) {
        return -1;
    }
    int vyhod = 0;
    if (kf_formula_apply(luchshaja, (int)znachenie, &vyhod) != 0) {
        return -1;
    }
    if (!skript->vyvod) {
        skript->vyvod = stdout;
    }
    fprintf(skript->vyvod, "[Скрипт] f(%ld) = %d\n", znachenie, vyhod);
    ks_zapisat_sobytiye(skript, "SCRIPT_ASK", "запрошено значение");
    return 0;
}

static int obrabotat_sohranit(KolibriScript *skript) {
    const KolibriFormula *luchshaja = kf_pool_best(skript->pool);
    if (!luchshaja) {
        return -1;
    }
    uint8_t cifry[64];
    size_t dlina = kf_formula_digits(luchshaja, cifry, sizeof(cifry));
    if (dlina == 0U || dlina >= sizeof(cifry)) {
        return -1;
    }
    char payload[128];
    size_t zapis = 0U;
    for (size_t indeks = 0; indeks < dlina && zapis + 1U < sizeof(payload); ++indeks) {
        payload[zapis++] = (char)('0' + cifry[indeks]);
    }
    payload[zapis] = '\0';
    ks_zapisat_sobytiye(skript, "SCRIPT_FORMULA", payload);
    if (!skript->vyvod) {
        skript->vyvod = stdout;
    }
    fprintf(skript->vyvod, "[Скрипт] формула сохранена в геноме\n");
    return 0;
}

static int vypolnit_stroku(KolibriScript *skript, char *stroka) {
    if (strncmp(stroka, "показать", strlen("показать")) == 0) {
        return obrabotat_pokazat(skript, stroka);
    }
    if (strncmp(stroka, "обучить", strlen("обучить")) == 0) {
        return obrabotat_obuchit(skript, stroka);
    }
    if (strncmp(stroka, "тикнуть", strlen("тикнуть")) == 0) {
        return obrabotat_tik(skript, stroka);
    }
    if (strncmp(stroka, "спросить", strlen("спросить")) == 0) {
        return obrabotat_spros(skript, stroka);
    }
    if (strncmp(stroka, "сохранить", strlen("сохранить")) == 0) {
        return obrabotat_sohranit(skript);
    }
    return -1;
}

int ks_execute(KolibriScript *skript) {
    if (!skript || !skript->pool) {
        return -1;
    }
    if (skript->potok.dlina == 0U) {
        return -1;
    }
    kolibri_potok_cifr_vernutsya(&skript->potok);
    size_t maks_dlina = kolibri_dlina_dekodirovki_teksta(skript->potok.dlina);
    unsigned char *tekst = (unsigned char *)calloc(maks_dlina + 1U, sizeof(unsigned char));
    if (!tekst) {
        return -1;
    }
    size_t zapisano = 0U;
    if (kolibri_izluchit_utf8(&skript->potok, tekst, maks_dlina, &zapisano) != 0) {
        free(tekst);
        return -1;
    }
    tekst[zapisano] = '\0';
    kolibri_potok_cifr_vernutsya(&skript->potok);
    ks_zapisat_sobytiye(skript, "SCRIPT_START", NULL);
    int status = 0;
    bool vnutri = false;
    char *saveptr = NULL;
    char *stroka = (char *)tekst;
    char *linija = strtok_r(stroka, "\r\n", &saveptr);
    while (linija) {
        ubrat_probel(linija);
        if (linija[0] == '\0') {
            linija = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }
        if (!vnutri) {
            if (strncmp(linija, "начало", strlen("начало")) == 0) {
                vnutri = true;
                linija = strtok_r(NULL, "\r\n", &saveptr);
                continue;
            }
            status = -1;
            break;
        }
        if (strncmp(linija, "конец", strlen("конец")) == 0) {
            vnutri = false;
            break;
        }
        if (vypolnit_stroku(skript, linija) != 0) {
            status = -1;
            ks_zapisat_sobytiye(skript, "SCRIPT_ERROR", linija);
            break;
        }
        linija = strtok_r(NULL, "\r\n", &saveptr);
    }
    free(tekst);
    if (vnutri) {
        status = -1;
    }
    if (status == 0) {
        ks_zapisat_sobytiye(skript, "SCRIPT_FINISH", NULL);
    }
    return status;
}
