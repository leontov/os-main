#ifndef KOLIBRI_GENOME_H
#define KOLIBRI_GENOME_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KOLIBRI_HASH_SIZE 32
#define KOLIBRI_EVENT_TYPE_SIZE 32
#define KOLIBRI_PAYLOAD_SIZE 256
#define KOLIBRI_HMAC_KEY_SIZE 64

typedef struct {
  uint64_t index;
  uint64_t timestamp;
  unsigned char prev_hash[KOLIBRI_HASH_SIZE];
  unsigned char hmac[KOLIBRI_HASH_SIZE];
  char event_type[KOLIBRI_EVENT_TYPE_SIZE];
  char payload[KOLIBRI_PAYLOAD_SIZE];
} ReasonBlock;

typedef struct {
  FILE *file;
  unsigned char last_hash[KOLIBRI_HASH_SIZE];
  unsigned char hmac_key[KOLIBRI_HMAC_KEY_SIZE];
  size_t hmac_key_len;
  char path[260];
  uint64_t next_index;
} KolibriGenome;

int kg_open(KolibriGenome *ctx, const char *path, const unsigned char *key,
            size_t key_len);
void kg_close(KolibriGenome *ctx);
int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload,
              ReasonBlock *out_block);
int kg_verify_file(const char *path, const unsigned char *key,
                   size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* KOLIBRI_GENOME_H */
