#include "support.h"

#include <stdint.h>

void *k_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void *k_memset(void *dest, int value, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    uint8_t byte = (uint8_t)value;
    for (size_t i = 0; i < n; ++i) {
        d[i] = byte;
    }
    return dest;
}

int k_memcmp(const void *lhs, const void *rhs, size_t n) {
    const uint8_t *a = (const uint8_t *)lhs;
    const uint8_t *b = (const uint8_t *)rhs;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

size_t k_strlen(const char *str) {
    size_t len = 0U;
    if (!str) {
        return 0U;
    }
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

char *k_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0U) {
        return dest;
    }
    size_t i = 0U;
    for (; i < n - 1U && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return dest;
}

size_t k_strlcpy(char *dest, const char *src, size_t n) {
    size_t src_len = k_strlen(src);
    if (!dest || n == 0U) {
        return src_len;
    }
    size_t copy_len = src_len;
    if (copy_len >= n) {
        copy_len = n - 1U;
    }
    for (size_t i = 0; i < copy_len; ++i) {
        dest[i] = src[i];
    }
    dest[copy_len] = '\0';
    return src_len;
}

int k_strcmp(const char *lhs, const char *rhs) {
    size_t i = 0U;
    while (lhs[i] != '\0' && rhs[i] != '\0') {
        if (lhs[i] != rhs[i]) {
            return (int)((unsigned char)lhs[i]) - (int)((unsigned char)rhs[i]);
        }
        ++i;
    }
    return (int)((unsigned char)lhs[i]) - (int)((unsigned char)rhs[i]);
}
