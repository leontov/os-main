/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#ifndef KOLIBRI_NET_H
#define KOLIBRI_NET_H

#include "kolibri/formula.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KOLIBRI_MSG_HELLO = 1,
    KOLIBRI_MSG_MIGRATE_RULE = 2,
    KOLIBRI_MSG_ACK = 3
} KolibriNetMessageType;

typedef struct {
    KolibriNetMessageType type;
    union {
        struct {
            uint32_t node_id;
        } hello;
        struct {
            uint32_t node_id;
            uint8_t length;
            uint8_t digits[32];
            double fitness;
        } formula;
        struct {
            uint8_t status;
        } ack;
    } data;
} KolibriNetMessage;

size_t kn_message_encode_hello(uint8_t *buffer, size_t buffer_len, uint32_t node_id);
size_t kn_message_encode_formula(uint8_t *buffer, size_t buffer_len, uint32_t node_id, const KolibriFormula *formula);
size_t kn_message_encode_ack(uint8_t *buffer, size_t buffer_len, uint8_t status);
int kn_message_decode(const uint8_t *buffer, size_t buffer_len, KolibriNetMessage *out_message);

int kn_share_formula(const char *host, uint16_t port, uint32_t node_id, const KolibriFormula *formula);

typedef struct {
    int socket_fd;
    uint16_t port;
} KolibriNetListener;

int kn_listener_start(KolibriNetListener *listener, uint16_t port);
int kn_listener_poll(KolibriNetListener *listener, uint32_t timeout_ms, KolibriNetMessage *out_message);
void kn_listener_close(KolibriNetListener *listener);

#ifdef __cplusplus
}
#endif

#endif /* KOLIBRI_NET_H */
