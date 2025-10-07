/*
 * Веб-сборка Kolibri не включает цифровой геном: для работы с HMAC
 * требуется OpenSSL, который недоступен в окружении Emscripten.
 * Чтобы ядро оставалось функциональным, мы предоставляем минимальные
 * заглушки API генома, используемые KolibriScript. При необходимости
 * полной поддержки установите OpenSSL и соберите с
 * KOLIBRI_WASM_INCLUDE_GENOME=1.
 */

#include "kolibri/genome.h"

int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload, ReasonBlock *out_block) {
    (void)ctx;
    (void)event_type;
    (void)payload;
    (void)out_block;
    return 0;
}

int kg_open(KolibriGenome *ctx, const char *path, const unsigned char *key, size_t key_len) {
    (void)ctx;
    (void)path;
    (void)key;
    (void)key_len;
    return -1;
}

void kg_close(KolibriGenome *ctx) {
    (void)ctx;
}

int kg_verify_file(const char *path, const unsigned char *key, size_t key_len) {
    (void)path;
    (void)key;
    (void)key_len;
    return -1;
}
