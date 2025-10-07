#include "kolibri/net.h"

#include "serial.h"
#include "support.h"

#define SLIP_END 0xC0U
#define SLIP_ESC 0xDBU
#define SLIP_ESC_END 0xDCU
#define SLIP_ESC_ESC 0xDDU

static const uint8_t LOCAL_IP[4] = {192U, 168U, 0U, 2U};

static void slip_send_byte(uint8_t byte) {
    if (byte == SLIP_END) {
        serial_write_char((char)SLIP_ESC);
        serial_write_char((char)SLIP_ESC_END);
    } else if (byte == SLIP_ESC) {
        serial_write_char((char)SLIP_ESC);
        serial_write_char((char)SLIP_ESC_ESC);
    } else {
        serial_write_char((char)byte);
    }
}

static uint16_t ip_checksum(const uint8_t *data, size_t length) {
    uint32_t sum = 0U;
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word = (uint16_t)((data[i] << 8) | data[i + 1]);
        sum += word;
    }
    if (length & 1U) {
        sum += (uint16_t)(data[length - 1] << 8);
    }
    while (sum >> 16U) {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return (uint16_t)(~sum);
}

static size_t build_ipv4_udp(const KolibriSlipUdp *ctx, const uint8_t *payload, size_t length, uint8_t *out) {
    const size_t ip_header = 20U;
    const size_t udp_header = 8U;
    size_t total = ip_header + udp_header + length;
    if (total > KOLIBRI_SLIP_MAX_PAYLOAD) {
        total = KOLIBRI_SLIP_MAX_PAYLOAD;
    }
    size_t payload_len = total - ip_header - udp_header;
    k_memset(out, 0, total);
    out[0] = 0x45U;
    out[1] = 0x00U;
    uint16_t total_len = (uint16_t)total;
    out[2] = (uint8_t)(total_len >> 8);
    out[3] = (uint8_t)(total_len & 0xFFU);
    out[8] = 64U;
    out[9] = 17U;
    k_memcpy(&out[12], LOCAL_IP, 4U);
    k_memcpy(&out[16], ctx->remote_ip, 4U);
    uint16_t checksum = ip_checksum(out, ip_header);
    out[10] = (uint8_t)(checksum >> 8);
    out[11] = (uint8_t)(checksum & 0xFFU);

    uint16_t src_port = ctx->local_port;
    uint16_t dst_port = ctx->remote_port;
    out[ip_header + 0] = (uint8_t)(src_port >> 8);
    out[ip_header + 1] = (uint8_t)(src_port & 0xFFU);
    out[ip_header + 2] = (uint8_t)(dst_port >> 8);
    out[ip_header + 3] = (uint8_t)(dst_port & 0xFFU);
    uint16_t udp_len = (uint16_t)(udp_header + payload_len);
    out[ip_header + 4] = (uint8_t)(udp_len >> 8);
    out[ip_header + 5] = (uint8_t)(udp_len & 0xFFU);
    out[ip_header + 6] = 0U;
    out[ip_header + 7] = 0U;

    for (size_t i = 0; i < payload_len; ++i) {
        out[ip_header + udp_header + i] = payload[i];
    }
    return total;
}

void kn_slip_udp_init(KolibriSlipUdp *ctx, uint16_t local_port) {
    if (!ctx) {
        return;
    }
    k_memset(ctx, 0, sizeof(*ctx));
    ctx->local_port = local_port;
    ctx->remote_port = local_port;
    ctx->remote_ip[0] = 192U;
    ctx->remote_ip[1] = 168U;
    ctx->remote_ip[2] = 0U;
    ctx->remote_ip[3] = 1U;
}

void kn_slip_udp_set_remote(KolibriSlipUdp *ctx, const uint8_t ip[4], uint16_t port) {
    if (!ctx || !ip) {
        return;
    }
    k_memcpy(ctx->remote_ip, ip, 4U);
    ctx->remote_port = port;
}

void kn_slip_udp_send(KolibriSlipUdp *ctx, const uint8_t *payload, size_t length) {
    if (!ctx || !payload || length == 0U) {
        return;
    }
    size_t packet_len = build_ipv4_udp(ctx, payload, length, ctx->tx_buffer);
    serial_write_char((char)SLIP_END);
    for (size_t i = 0; i < packet_len; ++i) {
        slip_send_byte(ctx->tx_buffer[i]);
    }
    serial_write_char((char)SLIP_END);
}

static size_t u32_to_dec(uint32_t value, char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0U) {
        return 0U;
    }
    char temp[16];
    size_t index = 0U;
    if (value == 0U) {
        temp[index++] = '0';
    } else {
        while (value > 0U && index < sizeof(temp)) {
            temp[index++] = (char)('0' + (value % 10U));
            value /= 10U;
        }
    }
    size_t written = 0U;
    while (written < index && written + 1U < buffer_len) {
        buffer[written] = temp[index - written - 1U];
        ++written;
    }
    if (written < buffer_len) {
        buffer[written++] = '\0';
    }
    return written;
}

void kn_slip_udp_send_hello(KolibriSlipUdp *ctx, uint32_t node_id) {
    if (!ctx) {
        return;
    }
    char message[64];
    k_strlcpy(message, "HELLO:", sizeof(message));
    u32_to_dec(node_id, message + 6, sizeof(message) - 6U);
    kn_slip_udp_send(ctx, (const uint8_t *)message, k_strlen(message));
}
