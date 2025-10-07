#ifndef KOLIBRI_KERNEL_RAMDISK_H
#define KOLIBRI_KERNEL_RAMDISK_H

#include <stddef.h>
#include <stdint.h>

#define KOLIBRI_RAMDISK_SIZE 4096U

void ramdisk_init(void);
uint8_t *ramdisk_data(void);
size_t ramdisk_capacity(void);
size_t ramdisk_size(void);
void ramdisk_commit(size_t used_bytes);

#endif /* KOLIBRI_KERNEL_RAMDISK_H */
