/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/script.h"
#include "kolibri/formula.h"
#include "kolibri/decimal.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_script(void) {
    KolibriFormulaPool pool;
    kf_pool_init(&pool, 424242ULL);

    KolibriScript skript;
    assert(ks_init(&skript, &pool, NULL) == 0);

    const char *programma =
        "начало:\n"
        "    показать \"Kolibri приветствует Архитектора\"\n"
        "    обучить число 2 -> 4\n"
        "    тикнуть 24\n"
        "    спросить число 2\n"
        "    сохранить лучшую формулу\n"
        "конец.\n";

    FILE *vyvod = tmpfile();
    assert(vyvod != NULL);
    ks_set_output(&skript, vyvod);

    assert(ks_load_text(&skript, programma) == 0);
    assert(ks_execute(&skript) == 0);

    fflush(vyvod);
    fseek(vyvod, 0L, SEEK_SET);
    char bufer[256];
    size_t prochitano = fread(bufer, 1U, sizeof(bufer) - 1U, vyvod);
    bufer[prochitano] = '\0';
    fclose(vyvod);

    ks_free(&skript);

    const KolibriFormula *luchshaja = kf_pool_best(&pool);
    assert(luchshaja != NULL);
    assert(strstr(bufer, "[Скрипт] f(2) =") != NULL);
}

static void zapisat_cifrovyj_skript(char *path, size_t path_dlina,
                                    const char *programma) {
    uint8_t bufer[16384];
    kolibri_potok_cifr potok;
    kolibri_potok_cifr_init(&potok, bufer, sizeof(bufer));
    assert(kolibri_transducirovat_utf8(&potok, (const unsigned char *)programma,
                                       strlen(programma)) == 0);
    char shablon[] = "/tmp/kolibri_scriptXXXXXX";
    assert(path_dlina >= sizeof(shablon));
    memcpy(path, shablon, sizeof(shablon));
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *file = fdopen(fd, "wb");
    assert(file != NULL);
    for (size_t indeks = 0; indeks < potok.dlina; ++indeks) {
        fputc('0' + potok.danniye[indeks], file);
    }
    fclose(file);
}

void test_script_load_digits(void) {
    KolibriFormulaPool pool;
    kf_pool_init(&pool, 171717ULL);

    KolibriScript skript;
    assert(ks_init(&skript, &pool, NULL) == 0);

    const char *programma =
        "начало:\n"
        "    показать \"Цифровой сценарий\"\n"
        "    обучить число 1 -> 3\n"
        "    тикнуть 8\n"
        "    спросить число 1\n"
        "конец.\n";

    char vremya[sizeof "/tmp/kolibri_scriptXXXXXX"];
    zapisat_cifrovyj_skript(vremya, sizeof(vremya), programma);

    assert(ks_load_file(&skript, vremya) == 0);
    assert(ks_execute(&skript) == 0);

    remove(vremya);
    ks_free(&skript);
}
