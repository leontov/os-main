#ifndef KOLIBRI_KERNEL_NET_H
#define KOLIBRI_KERNEL_NET_H

#include <stddef.h>
#include <stdint.h>

#define KOLIBRI_SLIP_MAX_PAYLOAD 256U

typedef struct {
    uint16_t local_port;
    uint16_t remote_port;
    uint8_t remote_ip[4];
    uint8_t tx_buffer[KOLIBRI_SLIP_MAX_PAYLOAD + 32U];
} KolibriSlipUdp;

void kn_slip_udp_init(KolibriSlipUdp *ctx, uint16_t local_port);
void kn_slip_udp_set_remote(KolibriSlipUdp *ctx, const uint8_t ip[4], uint16_t port);
void kn_slip_udp_send(KolibriSlipUdp *ctx, const uint8_t *payload, size_t length);
void kn_slip_udp_send_hello(KolibriSlipUdp *ctx, uint32_t node_id);

#endif /* KOLIBRI_KERNEL_NET_H */
