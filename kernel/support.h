#ifndef KOLIBRI_KERNEL_SUPPORT_H
#define KOLIBRI_KERNEL_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

void *k_memcpy(void *dest, const void *src, size_t n);
void *k_memset(void *dest, int value, size_t n);
int k_memcmp(const void *lhs, const void *rhs, size_t n);
size_t k_strlen(const char *str);
char *k_strncpy(char *dest, const char *src, size_t n);
size_t k_strlcpy(char *dest, const char *src, size_t n);
int k_strcmp(const char *lhs, const char *rhs);

static inline uint16_t k_htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint32_t k_htonl(uint32_t value) {
    return ((value & 0x000000FFU) << 24) |
           ((value & 0x0000FF00U) << 8) |
           ((value & 0x00FF0000U) >> 8) |
           ((value & 0xFF000000U) >> 24);
}

#endif /* KOLIBRI_KERNEL_SUPPORT_H */
