#ifndef KOLIBRI_ROY_H
#define KOLIBRI_ROY_H

#include "kolibri/formula.h"

#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KOLIBRI_ROY_MAGIC "KSP1"
#define KOLIBRI_ROY_MAX_SOSSEDI 64U
#define KOLIBRI_ROY_MAX_OCHERED 32U
#define KOLIBRI_ROY_HMAC_SIZE 32U
#define KOLIBRI_ROY_PRIVET_INTERVAL 5U
#define KOLIBRI_ROY_SROK_GODA 30U

typedef struct {
    uint32_t identifikator;
    struct sockaddr_in adres;
    time_t poslednij_otklik;
    uint32_t neudachi;
} KolibriRoySosed;

typedef enum {
    KOLIBRI_ROY_SOBYTIE_NONE = 0,
    KOLIBRI_ROY_SOBYTIE_HELLO = 1,
    KOLIBRI_ROY_SOBYTIE_FORMULA = 2
} KolibriRoySobytieTip;

typedef struct {
    KolibriRoySobytieTip tip;
    uint32_t identifikator;
    struct sockaddr_in adres;
    KolibriFormula formula;
} KolibriRoySobytie;

typedef struct {
    uint32_t sobstvennyj_id;
    uint16_t port;
    int soket;
    unsigned char klyuch[KOLIBRI_ROY_HMAC_SIZE];
    size_t dlina_klyucha;
    pthread_t potok;
    int zapushchen;
    pthread_mutex_t zamek;
    KolibriRoySosed sosedi[KOLIBRI_ROY_MAX_SOSSEDI];
    size_t chislo_sosedey;
    pthread_mutex_t ochered_zamek;
    KolibriRoySobytie ochered[KOLIBRI_ROY_MAX_OCHERED];
    size_t ochered_golova;
    size_t ochered_hvost;
    size_t ochered_schetchik;
    time_t poslednij_privet;
} KolibriRoy;

/* Инициализирует и запускает модуль роя на указанном UDP-порту. */
int kolibri_roy_zapustit(KolibriRoy *roy, uint32_t identifikator, uint16_t port,
        const unsigned char *klyuch, size_t dlina_klyucha);

/* Останавливает потоки и закрывает сокеты роя. */
void kolibri_roy_ostanovit(KolibriRoy *roy);

/* Возвращает очередное событие роя, если оно присутствует. */
int kolibri_roy_poluchit_sobytie(KolibriRoy *roy, KolibriRoySobytie *sobytie);

/* Возвращает копию списка соседей в предоставленный буфер. */
size_t kolibri_roy_spisok_sosedey(KolibriRoy *roy, KolibriRoySosed *naznachenie,
        size_t maksimalno);

/* Отправляет широковещательное приветствие в сеть. */
int kolibri_roy_otpravit_privet(KolibriRoy *roy);

/* Добавляет статического соседа и отправляет ему приветствие. */
int kolibri_roy_dobavit_soseda(KolibriRoy *roy, const struct sockaddr_in *adres,
        uint32_t identifikator);

/* Отправляет формулу случайному соседу, используя внешнее случайное число. */
int kolibri_roy_otpravit_sluchajnomu(KolibriRoy *roy, uint64_t sluchajnoe,
        const KolibriFormula *formula);

/* Рассылает формулу всем соседям и широковещательно. */
int kolibri_roy_otpravit_vsem(KolibriRoy *roy, const KolibriFormula *formula);

#ifdef __cplusplus
}
#endif

#endif /* KOLIBRI_ROY_H */
