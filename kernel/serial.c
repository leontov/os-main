#include "serial.h"

static inline void io_out8(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#define COM1_BASE 0x3F8U

void serial_init(uint32_t baud_divisor) {
    io_out8(COM1_BASE + 1U, 0x00U);
    io_out8(COM1_BASE + 3U, 0x80U);
    io_out8(COM1_BASE + 0U, (uint8_t)(baud_divisor & 0xFFU));
    io_out8(COM1_BASE + 1U, (uint8_t)((baud_divisor >> 8U) & 0xFFU));
    io_out8(COM1_BASE + 3U, 0x03U);
    io_out8(COM1_BASE + 2U, 0xC7U);
    io_out8(COM1_BASE + 4U, 0x0BU);
}

static void serial_wait_tx(void) {
    while ((io_in8(COM1_BASE + 5U) & 0x20U) == 0U) {
    }
}

void serial_write_char(char c) {
    if (c == '\n') {
        serial_write_char('\r');
    }
    serial_wait_tx();
    io_out8(COM1_BASE, (uint8_t)c);
}

void serial_write_string(const char *str) {
    if (!str) {
        return;
    }
    while (*str) {
        serial_write_char(*str++);
    }
}

static char to_hex(uint8_t nibble) {
    nibble &= 0x0FU;
    return (char)((nibble < 10U) ? ('0' + nibble) : ('A' + (nibble - 10U)));
}

void serial_write_hex32(uint32_t value) {
    serial_write_string("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        serial_write_char(to_hex((uint8_t)((value >> shift) & 0x0FU)));
    }
}

int serial_read_char(char *out) {
    if (!out) {
        return -1;
    }
    if ((io_in8(COM1_BASE + 5U) & 0x01U) == 0U) {
        return 1;
    }
    *out = (char)io_in8(COM1_BASE);
    return 0;
}
