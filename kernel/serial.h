#ifndef KOLIBRI_KERNEL_SERIAL_H
#define KOLIBRI_KERNEL_SERIAL_H

#include <stddef.h>
#include <stdint.h>

void serial_init(uint32_t baud_divisor);
void serial_write_char(char c);
void serial_write_string(const char *str);
void serial_write_hex32(uint32_t value);
int serial_read_char(char *out);

#endif /* KOLIBRI_KERNEL_SERIAL_H */
