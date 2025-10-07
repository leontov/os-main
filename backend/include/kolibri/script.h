/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#ifndef KOLIBRI_SCRIPT_H
#define KOLIBRI_SCRIPT_H

#include "kolibri/digits.h"
#include "kolibri/decimal.h"
#include "kolibri/formula.h"
#include "kolibri/genome.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Контекст исполнения KolibriScript. Хранит цифровой поток сценария и
 * предоставляет доступ к пулу формул и цифровому геному.
 */
typedef struct {
    kolibri_potok_cifr potok;
    uint8_t *hranilishe;
    size_t emkost;
    KolibriFormulaPool *pool;
    KolibriGenome *genome;
    FILE *vyvod;
} KolibriScript;

/* Инициализирует интерпретатор и выделяет внутренний цифровой буфер. */
int ks_init(KolibriScript *skript, KolibriFormulaPool *pool,
            KolibriGenome *genome);

/* Освобождает выделенные ресурсы интерпретатора. */
void ks_free(KolibriScript *skript);

/* Переназначает поток вывода интерпретатора (по умолчанию stdout). */
void ks_set_output(KolibriScript *skript, FILE *vyvod);

/* Загружает русскоязычный сценарий из текстовой строки. */
int ks_load_text(KolibriScript *skript, const char *text);

/* Загружает сценарий из файла на диске. */
int ks_load_file(KolibriScript *skript, const char *path);

/* Выполняет сценарий, возвращает 0 при успехе. */
int ks_execute(KolibriScript *skript);

#ifdef __cplusplus
}
#endif

#endif /* KOLIBRI_SCRIPT_H */
