#include "ramdisk.h"

#include "support.h"

#define RAMDISK_MAGIC 0x4B4F474EU /* 'KOGN' */

static uint8_t ramdisk_buffer[KOLIBRI_RAMDISK_SIZE];
static size_t ramdisk_used = 0U;

void ramdisk_init(void) {
    uint32_t stored_magic = 0U;
    k_memcpy(&stored_magic, ramdisk_buffer, sizeof(stored_magic));
    if (stored_magic != RAMDISK_MAGIC) {
        k_memset(ramdisk_buffer, 0, sizeof(ramdisk_buffer));
        stored_magic = RAMDISK_MAGIC;
        k_memcpy(ramdisk_buffer, &stored_magic, sizeof(stored_magic));
        ramdisk_used = 0U;
    } else {
        k_memcpy(&ramdisk_used, ramdisk_buffer + sizeof(stored_magic), sizeof(ramdisk_used));
        if (ramdisk_used > KOLIBRI_RAMDISK_SIZE - sizeof(stored_magic) - sizeof(ramdisk_used)) {
            ramdisk_used = 0U;
        }
    }
    size_t capacity = ramdisk_capacity();
    if (ramdisk_used < capacity) {
        k_memset(ramdisk_data() + ramdisk_used, 0, capacity - ramdisk_used);
    }
}

uint8_t *ramdisk_data(void) {
    return ramdisk_buffer + sizeof(uint32_t) + sizeof(size_t);
}

size_t ramdisk_capacity(void) {
    return KOLIBRI_RAMDISK_SIZE - sizeof(uint32_t) - sizeof(size_t);
}

size_t ramdisk_size(void) {
    return ramdisk_used;
}

void ramdisk_commit(size_t used_bytes) {
    size_t capacity = ramdisk_capacity();
    if (used_bytes > capacity) {
        used_bytes = capacity;
    }
    ramdisk_used = used_bytes;
    k_memcpy(ramdisk_buffer, &(uint32_t){RAMDISK_MAGIC}, sizeof(uint32_t));
    k_memcpy(ramdisk_buffer + sizeof(uint32_t), &ramdisk_used, sizeof(ramdisk_used));
    if (ramdisk_used < capacity) {
        k_memset(ramdisk_data() + ramdisk_used, 0, capacity - ramdisk_used);
    }
}
