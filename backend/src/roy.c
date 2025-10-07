/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/roy.h"

#include <arpa/inet.h>
#include <errno.h>
#include <openssl/hmac.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define KOLIBRI_ROY_VERSIYA 1U
#define KOLIBRI_ROY_TYP_HELLO 1U
#define KOLIBRI_ROY_TYP_FORMULA 2U
#define KOLIBRI_ROY_MAKSIMALNYJ_PAKET 512U

/* Преобразует число из хоста в сетевой порядок для 64 бит. */
static uint64_t kolibri_htonll(uint64_t znachenie) {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)htonl((uint32_t)(znachenie & 0xFFFFFFFFULL))) << 32) |
           htonl((uint32_t)(znachenie >> 32));
#else
    return znachenie;
#endif
}

/* Преобразует число из сетевого порядка в порядок хоста для 64 бит. */
static uint64_t kolibri_ntohll(uint64_t znachenie) {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)ntohl((uint32_t)(znachenie & 0xFFFFFFFFULL))) << 32) |
           ntohl((uint32_t)(znachenie >> 32));
#else
    return znachenie;
#endif
}

/* Внутренний помощник для записи события в очередь. */
static void kolibri_roy_postavit_sobytie(KolibriRoy *roy,
                                         const KolibriRoySobytie *sobytie) {

    pthread_mutex_lock(&roy->ochered_zamek);
    if (roy->ochered_schetchik == KOLIBRI_ROY_MAX_OCHERED) {
        /* Переполнение очереди: перезаписываем самое старое событие. */
        roy->ochered_golova =
            (roy->ochered_golova + 1U) % KOLIBRI_ROY_MAX_OCHERED;
        roy->ochered_schetchik--;
    }
    roy->ochered[roy->ochered_hvost] = *sobytie;
    roy->ochered_hvost = (roy->ochered_hvost + 1U) % KOLIBRI_ROY_MAX_OCHERED;
    roy->ochered_schetchik++;
    pthread_mutex_unlock(&roy->ochered_zamek);
}

/* Сравнивает два HMAC и защищает от атак по времени. */
static int kolibri_roy_sravnit_hmac(const unsigned char *levyj,
                                    const unsigned char *pravyj) {

    unsigned char rezultat = 0U;
    for (size_t indeks = 0; indeks < KOLIBRI_ROY_HMAC_SIZE; ++indeks) {
        rezultat |= (unsigned char)(levyj[indeks] ^ pravyj[indeks]);
    }
    return rezultat == 0U ? 0 : -1;
}

/* Обновляет или создаёт запись о соседе. */
static void kolibri_roy_obnovit_soseda(KolibriRoy *roy, uint32_t identifikator,
                                       const struct sockaddr_in *adres) {

    pthread_mutex_lock(&roy->zamek);
    size_t indeks = 0U;
    for (; indeks < roy->chislo_sosedey; ++indeks) {
        if (roy->sosedi[indeks].identifikator == identifikator ||
            (roy->sosedi[indeks].adres.sin_addr.s_addr ==
                 adres->sin_addr.s_addr &&
             roy->sosedi[indeks].adres.sin_port == adres->sin_port)) {
            roy->sosedi[indeks].identifikator = identifikator;
            roy->sosedi[indeks].adres = *adres;
            roy->sosedi[indeks].poslednij_otklik = time(NULL);
            roy->sosedi[indeks].neudachi = 0U;
            pthread_mutex_unlock(&roy->zamek);
            return;
        }
    }
    if (roy->chislo_sosedey < KOLIBRI_ROY_MAX_SOSSEDI) {
        roy->sosedi[roy->chislo_sosedey].identifikator = identifikator;
        roy->sosedi[roy->chislo_sosedey].adres = *adres;
        roy->sosedi[roy->chislo_sosedey].poslednij_otklik = time(NULL);
        roy->sosedi[roy->chislo_sosedey].neudachi = 0U;
        roy->chislo_sosedey++;
    }
    pthread_mutex_unlock(&roy->zamek);
}

/* Удаляет устаревших соседей. */
static void kolibri_roy_ochistit_sosedey(KolibriRoy *roy) {

    const time_t seichas = time(NULL);
    pthread_mutex_lock(&roy->zamek);
    size_t zapis = 0U;
    for (size_t indeks = 0U; indeks < roy->chislo_sosedey; ++indeks) {
        time_t poslednij = roy->sosedi[indeks].poslednij_otklik;
        if (poslednij + (time_t)KOLIBRI_ROY_SROK_GODA < seichas) {
            continue;
        }
        if (zapis != indeks) {
            roy->sosedi[zapis] = roy->sosedi[indeks];
        }
        zapis++;
    }
    roy->chislo_sosedey = zapis;
    pthread_mutex_unlock(&roy->zamek);
}

/* Формирует общий заголовок KSP-сообщения. */
static size_t kolibri_roy_zapolnit_zagolovok(const KolibriRoy *roy, uint8_t tip,
                                             uint8_t *buffer, size_t razmer,
                                             uint16_t payload) {

    if (!buffer || razmer < 12U) {
        return 0U;
    }
    size_t offset = 0U;
    memcpy(buffer + offset, KOLIBRI_ROY_MAGIC, 4U);
    offset += 4U;
    buffer[offset++] = (uint8_t)KOLIBRI_ROY_VERSIYA;
    buffer[offset++] = tip;
    uint32_t id = htonl(roy->sobstvennyj_id);
    memcpy(buffer + offset, &id, sizeof(id));
    offset += sizeof(id);
    uint16_t port = htons(roy->port);
    memcpy(buffer + offset, &port, sizeof(port));
    offset += sizeof(port);
    uint16_t dlina = htons(payload);
    memcpy(buffer + offset, &dlina, sizeof(dlina));
    offset += sizeof(dlina);
    return offset;
}

/* Вычисляет и присоединяет HMAC к концу пакета. */
static size_t kolibri_roy_prisoedinit_hmac(const KolibriRoy *roy,
                                           uint8_t *buffer,
                                           size_t tekushchaya_dlina) {

    unsigned int hmac_dlina = 0U;
    unsigned char hmac[KOLIBRI_ROY_HMAC_SIZE];
    unsigned char *result =
        HMAC(EVP_sha256(), roy->klyuch, (int)roy->dlina_klyucha, buffer,
             tekushchaya_dlina, hmac, &hmac_dlina);
    if (!result || hmac_dlina < KOLIBRI_ROY_HMAC_SIZE) {
        return 0U;
    }
    memcpy(buffer + tekushchaya_dlina, hmac, KOLIBRI_ROY_HMAC_SIZE);
    return tekushchaya_dlina + KOLIBRI_ROY_HMAC_SIZE;
}

/* Рассылает пакет по заданному адресу. */
static int kolibri_roy_otpravit_paket(const KolibriRoy *roy,
                                      const struct sockaddr_in *adres,
                                      const uint8_t *buffer, size_t dlina) {

    ssize_t otpravleno = sendto(roy->soket, buffer, dlina, 0,
                                (const struct sockaddr *)adres, sizeof(*adres));
    if (otpravleno < 0 || (size_t)otpravleno != dlina) {
        return -1;
    }
    return 0;
}

/* Формирует широковещательный адрес. */
static void kolibri_roy_shirokoveshchatel(struct sockaddr_in *adres,
                                          uint16_t port) {

    memset(adres, 0, sizeof(*adres));
    adres->sin_family = AF_INET;
    adres->sin_port = htons(port);
    adres->sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

/* Собирает и отправляет приветственное сообщение. */
static int
kolibri_roy_soobshchenie_privet(KolibriRoy *roy,
                                const struct sockaddr_in *naznachenie) {

    uint8_t paket[KOLIBRI_ROY_MAKSIMALNYJ_PAKET];
    size_t zagolovok = kolibri_roy_zapolnit_zagolovok(
        roy, KOLIBRI_ROY_TYP_HELLO, paket, sizeof(paket), 0U);
    if (zagolovok == 0U) {
        return -1;
    }
    size_t polnaja_dlina = kolibri_roy_prisoedinit_hmac(roy, paket, zagolovok);
    if (polnaja_dlina == 0U) {
        return -1;
    }
    return kolibri_roy_otpravit_paket(roy, naznachenie, paket, polnaja_dlina);
}

/* Собирает и отправляет формулу. */
static int
kolibri_roy_soobshchenie_formula(KolibriRoy *roy,
                                 const struct sockaddr_in *naznachenie,
                                 const KolibriFormula *formula) {

    uint8_t paket[KOLIBRI_ROY_MAKSIMALNYJ_PAKET];
    uint8_t payload[64];
    size_t offset = 0U;
    if (!formula) {
        return -1;
    }
    uint8_t dlina = (uint8_t)formula->gene.length;
    if (dlina == 0U || dlina > sizeof(formula->gene.digits)) {
        return -1;
    }
    payload[offset++] = dlina;
    memcpy(payload + offset, formula->gene.digits, dlina);
    offset += dlina;
    uint64_t kody;
    memcpy(&kody, &formula->fitness, sizeof(kody));
    kody = kolibri_htonll(kody);
    memcpy(payload + offset, &kody, sizeof(kody));
    offset += sizeof(kody);

    size_t zagolovok = kolibri_roy_zapolnit_zagolovok(
        roy, KOLIBRI_ROY_TYP_FORMULA, paket, sizeof(paket), (uint16_t)offset);
    if (zagolovok == 0U ||
        zagolovok + offset + KOLIBRI_ROY_HMAC_SIZE > sizeof(paket)) {
        return -1;
    }
    memcpy(paket + zagolovok, payload, offset);
    size_t polnaja_dlina =
        kolibri_roy_prisoedinit_hmac(roy, paket, zagolovok + offset);
    if (polnaja_dlina == 0U) {
        return -1;
    }
    return kolibri_roy_otpravit_paket(roy, naznachenie, paket, polnaja_dlina);
}

/* Главная петля фонового потока: слушает UDP и отправляет приветствия. */
static void *kolibri_roy_potok(void *argument) {

    KolibriRoy *roy = (KolibriRoy *)argument;
    struct sockaddr_in otkuda;
    socklen_t otkuda_dlina = sizeof(otkuda);
    uint8_t paket[KOLIBRI_ROY_MAKSIMALNYJ_PAKET];
    while (roy->zapushchen) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_set nabor;
        FD_ZERO(&nabor);
        FD_SET(roy->soket, &nabor);
        int gotov = select(roy->soket + 1, &nabor, NULL, NULL, &tv);
        if (!roy->zapushchen) {
            break;
        }
        if (gotov < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }
        if (gotov > 0 && FD_ISSET(roy->soket, &nabor)) {
            ssize_t prinyato =
                recvfrom(roy->soket, paket, sizeof(paket), 0,
                         (struct sockaddr *)&otkuda, &otkuda_dlina);
            if (prinyato <= 0) {
                continue;
            }
            if ((size_t)prinyato <= KOLIBRI_ROY_HMAC_SIZE + 10U) {
                continue;
            }
            size_t dlina = (size_t)prinyato;
            unsigned char prisoyedennyj[KOLIBRI_ROY_HMAC_SIZE];
            memcpy(prisoyedennyj, paket + dlina - KOLIBRI_ROY_HMAC_SIZE,
                   KOLIBRI_ROY_HMAC_SIZE);
            dlina -= KOLIBRI_ROY_HMAC_SIZE;
            unsigned int hmac_dlina = 0U;
            unsigned char rasschet[KOLIBRI_ROY_HMAC_SIZE];
            unsigned char *result =
                HMAC(EVP_sha256(), roy->klyuch, (int)roy->dlina_klyucha, paket,
                     dlina, rasschet, &hmac_dlina);
            if (!result || hmac_dlina < KOLIBRI_ROY_HMAC_SIZE) {
                continue;
            }
            if (kolibri_roy_sravnit_hmac(prisoyedennyj, rasschet) != 0) {
                continue;
            }
            if (memcmp(paket, KOLIBRI_ROY_MAGIC, 4U) != 0) {
                continue;
            }
            uint8_t versiya = paket[4];
            if (versiya != KOLIBRI_ROY_VERSIYA) {
                continue;
            }
            uint8_t tip = paket[5];
            uint32_t identifikator;
            memcpy(&identifikator, paket + 6U, sizeof(identifikator));
            identifikator = ntohl(identifikator);
            if (identifikator == roy->sobstvennyj_id) {
                continue;
            }
            uint16_t port;
            memcpy(&port, paket + 10U, sizeof(port));
            port = ntohs(port);
            uint16_t payload;
            memcpy(&payload, paket + 12U, sizeof(payload));
            payload = ntohs(payload);
            if (12U + payload > dlina) {
                continue;
            }
            otkuda.sin_port = htons(port);
            kolibri_roy_obnovit_soseda(roy, identifikator, &otkuda);
            KolibriRoySobytie sobytie;
            memset(&sobytie, 0, sizeof(sobytie));
            sobytie.identifikator = identifikator;
            sobytie.adres = otkuda;
            if (tip == KOLIBRI_ROY_TYP_HELLO) {
                sobytie.tip = KOLIBRI_ROY_SOBYTIE_HELLO;
                kolibri_roy_postavit_sobytie(roy, &sobytie);
            } else if (tip == KOLIBRI_ROY_TYP_FORMULA) {
                if (payload < 1U + sizeof(uint64_t)) {
                    continue;
                }
                const uint8_t *dannye = paket + 14U;
                uint8_t dlina_gena = dannye[0];
                if (dlina_gena == 0U || dlina_gena > 32U ||
                    payload < 1U + dlina_gena + sizeof(uint64_t)) {
                    continue;
                }
                memcpy(sobytie.formula.gene.digits, dannye + 1U, dlina_gena);
                sobytie.formula.gene.length = dlina_gena;
                uint64_t syrjoj;
                memcpy(&syrjoj, dannye + 1U + dlina_gena, sizeof(syrjoj));
                syrjoj = kolibri_ntohll(syrjoj);
                memcpy(&sobytie.formula.fitness, &syrjoj, sizeof(syrjoj));
                sobytie.formula.feedback = 0.0;
                sobytie.tip = KOLIBRI_ROY_SOBYTIE_FORMULA;
                kolibri_roy_postavit_sobytie(roy, &sobytie);
            }
        }
        time_t seichas = time(NULL);
        if (seichas - roy->poslednij_privet >=
            (time_t)KOLIBRI_ROY_PRIVET_INTERVAL) {
            struct sockaddr_in broadcast;
            kolibri_roy_shirokoveshchatel(&broadcast, roy->port);
            kolibri_roy_soobshchenie_privet(roy, &broadcast);
            roy->poslednij_privet = seichas;
        }
        kolibri_roy_ochistit_sosedey(roy);
    }
    return NULL;
}

int kolibri_roy_zapustit(KolibriRoy *roy, uint32_t identifikator, uint16_t port,
                         const unsigned char *klyuch, size_t dlina_klyucha) {

    if (!roy || !klyuch || dlina_klyucha == 0U) {
        return -1;
    }
    memset(roy, 0, sizeof(*roy));
    roy->sobstvennyj_id = identifikator;
    roy->port = port;
    roy->dlina_klyucha = dlina_klyucha > KOLIBRI_ROY_HMAC_SIZE
                             ? KOLIBRI_ROY_HMAC_SIZE
                             : dlina_klyucha;
    memcpy(roy->klyuch, klyuch, roy->dlina_klyucha);
    pthread_mutex_init(&roy->zamek, NULL);
    pthread_mutex_init(&roy->ochered_zamek, NULL);
    roy->soket = socket(AF_INET, SOCK_DGRAM, 0);
    if (roy->soket < 0) {
        return -1;
    }
    int reuse = 1;
    if (setsockopt(roy->soket, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0) {
        close(roy->soket);
        return -1;
    }
    int broadcast = 1;
    if (setsockopt(roy->soket, SOL_SOCKET, SO_BROADCAST, &broadcast,
                   sizeof(broadcast)) < 0) {
        close(roy->soket);
        return -1;
    }
    struct sockaddr_in adres;
    memset(&adres, 0, sizeof(adres));
    adres.sin_family = AF_INET;
    adres.sin_addr.s_addr = htonl(INADDR_ANY);
    adres.sin_port = htons(port);
    if (bind(roy->soket, (struct sockaddr *)&adres, sizeof(adres)) < 0) {
        close(roy->soket);
        return -1;
    }
    roy->zapushchen = 1;
    roy->poslednij_privet = time(NULL);
    if (pthread_create(&roy->potok, NULL, kolibri_roy_potok, roy) != 0) {
        close(roy->soket);
        roy->zapushchen = 0;
        return -1;
    }
    struct sockaddr_in broadcast_adres;
    kolibri_roy_shirokoveshchatel(&broadcast_adres, port);
    kolibri_roy_soobshchenie_privet(roy, &broadcast_adres);
    return 0;
}

void kolibri_roy_ostanovit(KolibriRoy *roy) {

    if (!roy) {
        return;
    }
    roy->zapushchen = 0;
    if (roy->soket >= 0) {
        shutdown(roy->soket, SHUT_RDWR);
    }
    if (roy->potok) {
        pthread_join(roy->potok, NULL);
    }
    if (roy->soket >= 0) {
        close(roy->soket);
        roy->soket = -1;
    }
    pthread_mutex_destroy(&roy->zamek);
    pthread_mutex_destroy(&roy->ochered_zamek);
}

int kolibri_roy_poluchit_sobytie(KolibriRoy *roy, KolibriRoySobytie *sobytie) {

    if (!roy || !sobytie) {
        return -1;
    }
    pthread_mutex_lock(&roy->ochered_zamek);
    if (roy->ochered_schetchik == 0U) {
        pthread_mutex_unlock(&roy->ochered_zamek);
        return 0;
    }
    *sobytie = roy->ochered[roy->ochered_golova];
    roy->ochered_golova = (roy->ochered_golova + 1U) % KOLIBRI_ROY_MAX_OCHERED;
    roy->ochered_schetchik--;
    pthread_mutex_unlock(&roy->ochered_zamek);
    return 1;
}

size_t kolibri_roy_spisok_sosedey(KolibriRoy *roy, KolibriRoySosed *naznachenie,
                                  size_t maksimalno) {

    if (!roy || !naznachenie || maksimalno == 0U) {
        return 0U;
    }
    pthread_mutex_lock(&roy->zamek);
    size_t kopiruem =
        roy->chislo_sosedey < maksimalno ? roy->chislo_sosedey : maksimalno;
    for (size_t indeks = 0U; indeks < kopiruem; ++indeks) {
        naznachenie[indeks] = roy->sosedi[indeks];
    }
    pthread_mutex_unlock(&roy->zamek);
    return kopiruem;
}

int kolibri_roy_otpravit_privet(KolibriRoy *roy) {

    if (!roy) {
        return -1;
    }
    struct sockaddr_in broadcast_adres;
    kolibri_roy_shirokoveshchatel(&broadcast_adres, roy->port);
    roy->poslednij_privet = time(NULL);
    return kolibri_roy_soobshchenie_privet(roy, &broadcast_adres);
}

int kolibri_roy_dobavit_soseda(KolibriRoy *roy, const struct sockaddr_in *adres,
                               uint32_t identifikator) {

    if (!roy || !adres) {
        return -1;
    }
    kolibri_roy_obnovit_soseda(roy, identifikator, adres);
    return kolibri_roy_soobshchenie_privet(roy, adres);
}

int kolibri_roy_otpravit_sluchajnomu(KolibriRoy *roy, uint64_t sluchajnoe,
                                     const KolibriFormula *formula) {

    if (!roy || !formula) {
        return -1;
    }
    pthread_mutex_lock(&roy->zamek);
    if (roy->chislo_sosedey == 0U) {
        pthread_mutex_unlock(&roy->zamek);
        return -1;
    }
    size_t indeks = (size_t)(sluchajnoe % roy->chislo_sosedey);
    KolibriRoySosed sosed = roy->sosedi[indeks];
    pthread_mutex_unlock(&roy->zamek);
    if (kolibri_roy_soobshchenie_formula(roy, &sosed.adres, formula) != 0) {
        return -1;
    }
    return 0;
}

int kolibri_roy_otpravit_vsem(KolibriRoy *roy, const KolibriFormula *formula) {

    if (!roy || !formula) {
        return -1;
    }
    struct sockaddr_in broadcast_adres;
    kolibri_roy_shirokoveshchatel(&broadcast_adres, roy->port);
    kolibri_roy_soobshchenie_formula(roy, &broadcast_adres, formula);
    pthread_mutex_lock(&roy->zamek);
    size_t chislo = roy->chislo_sosedey;
    KolibriRoySosed lokalnye[KOLIBRI_ROY_MAX_SOSSEDI];
    for (size_t indeks = 0U; indeks < chislo; ++indeks) {
        lokalnye[indeks] = roy->sosedi[indeks];
    }
    pthread_mutex_unlock(&roy->zamek);
    for (size_t indeks = 0U; indeks < chislo; ++indeks) {
        kolibri_roy_soobshchenie_formula(roy, &lokalnye[indeks].adres, formula);
    }
    return 0;
}
