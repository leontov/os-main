/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/genome.h"

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void bytes_to_hex(const unsigned char *bytes, size_t len, char *out,
                         size_t out_len) {
  static const char hex[] = "0123456789abcdef";
  if (out_len < len * 2 + 1) {
    return;
  }
  for (size_t i = 0; i < len; ++i) {
    out[i * 2] = hex[(bytes[i] >> 4) & 0xF];
    out[i * 2 + 1] = hex[bytes[i] & 0xF];
  }
  out[len * 2] = '\0';
}

static int hex_to_bytes(const char *hex, unsigned char *out, size_t out_len) {
  size_t len = strlen(hex);
  if (len % 2 != 0 || len / 2 != out_len) {
    return -1;
  }
  for (size_t i = 0; i < out_len; ++i) {
    unsigned int value = 0;
    if (sscanf(hex + i * 2, "%02x", &value) != 1) {
      return -1;
    }
    out[i] = (unsigned char)value;
  }
  return 0;
}

static void reset_context(KolibriGenome *ctx) {
  if (!ctx) {
    return;
  }
  ctx->file = NULL;
  memset(ctx->last_hash, 0, sizeof(ctx->last_hash));
  memset(ctx->hmac_key, 0, sizeof(ctx->hmac_key));
  ctx->hmac_key_len = 0;
  memset(ctx->path, 0, sizeof(ctx->path));
  ctx->next_index = 0;
}

int kg_open(KolibriGenome *ctx, const char *path, const unsigned char *key,
            size_t key_len) {
  if (!ctx || !path || !key || key_len == 0 ||
      key_len > sizeof(ctx->hmac_key)) {
    return -1;
  }

  reset_context(ctx);

  ctx->file = fopen(path, "a+b");
  if (!ctx->file) {
    return -1;
  }

  strncpy(ctx->path, path, sizeof(ctx->path) - 1);
  memcpy(ctx->hmac_key, key, key_len);
  ctx->hmac_key_len = key_len;

  fflush(ctx->file);
  fseek(ctx->file, 0, SEEK_SET);

  char line[1024];
  ReasonBlock block;
  while (fgets(line, sizeof(line), ctx->file)) {
    char prev_hash_hex[KOLIBRI_HASH_SIZE * 2 + 1];
    char hmac_hex[KOLIBRI_HASH_SIZE * 2 + 1];
    char event[KOLIBRI_EVENT_TYPE_SIZE];
    char payload[KOLIBRI_PAYLOAD_SIZE];
    unsigned long long index = 0ULL;
    unsigned long long timestamp = 0ULL;
    int matched =
        sscanf(line, "%llu,%llu,%64[^,],%64[^,],%31[^,],%255[^\n]", &index,
               &timestamp, prev_hash_hex, hmac_hex, event, payload);
    if (matched == 6) {
      block.index = (uint64_t)index;
      block.timestamp = (uint64_t)timestamp;
      strncpy(block.event_type, event, sizeof(block.event_type) - 1);
      block.event_type[sizeof(block.event_type) - 1] = '\0';
      strncpy(block.payload, payload, sizeof(block.payload) - 1);
      block.payload[sizeof(block.payload) - 1] = '\0';
      if (hex_to_bytes(prev_hash_hex, block.prev_hash, KOLIBRI_HASH_SIZE) ==
              0 &&
          hex_to_bytes(hmac_hex, block.hmac, KOLIBRI_HASH_SIZE) == 0) {
        memcpy(ctx->last_hash, block.hmac, KOLIBRI_HASH_SIZE);
        ctx->next_index = block.index + 1;
      }
    }
  }

  fseek(ctx->file, 0, SEEK_END);
  return 0;
}

void kg_close(KolibriGenome *ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->file) {
    fclose(ctx->file);
    ctx->file = NULL;
  }
  memset(ctx->last_hash, 0, sizeof(ctx->last_hash));
  memset(ctx->hmac_key, 0, sizeof(ctx->hmac_key));
  ctx->hmac_key_len = 0;
  memset(ctx->path, 0, sizeof(ctx->path));
  ctx->next_index = 0;
}

static void build_payload_buffer(const ReasonBlock *block,
                                 unsigned char *buffer, size_t *out_len) {
  size_t offset = 0;
  memcpy(buffer + offset, &block->index, sizeof(block->index));
  offset += sizeof(block->index);
  memcpy(buffer + offset, &block->timestamp, sizeof(block->timestamp));
  offset += sizeof(block->timestamp);
  memcpy(buffer + offset, block->prev_hash, KOLIBRI_HASH_SIZE);
  offset += KOLIBRI_HASH_SIZE;
  memcpy(buffer + offset, block->event_type, sizeof(block->event_type));
  offset += sizeof(block->event_type);
  memcpy(buffer + offset, block->payload, sizeof(block->payload));
  offset += sizeof(block->payload);
  *out_len = offset;
}

int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload,
              ReasonBlock *out_block) {
  if (!ctx || !ctx->file || !event_type || !payload) {
    return -1;
  }

  ReasonBlock block;
  memset(&block, 0, sizeof(block));
  block.index = ctx->next_index++;
  block.timestamp = (uint64_t)time(NULL);
  memcpy(block.prev_hash, ctx->last_hash, KOLIBRI_HASH_SIZE);
  strncpy(block.event_type, event_type, sizeof(block.event_type) - 1);
  strncpy(block.payload, payload, sizeof(block.payload) - 1);

  unsigned char buffer[sizeof(block.index) + sizeof(block.timestamp) +
                       KOLIBRI_HASH_SIZE + sizeof(block.event_type) +
                       sizeof(block.payload)];
  size_t buffer_len = 0;
  build_payload_buffer(&block, buffer, &buffer_len);

  unsigned int hmac_len = 0;
  unsigned char *result =
      HMAC(EVP_sha256(), ctx->hmac_key, (int)ctx->hmac_key_len, buffer,
           buffer_len, block.hmac, &hmac_len);
  if (!result || hmac_len != KOLIBRI_HASH_SIZE) {
    return -1;
  }

  memcpy(ctx->last_hash, block.hmac, KOLIBRI_HASH_SIZE);

  char prev_hex[KOLIBRI_HASH_SIZE * 2 + 1];
  char hmac_hex[KOLIBRI_HASH_SIZE * 2 + 1];
  bytes_to_hex(block.prev_hash, KOLIBRI_HASH_SIZE, prev_hex, sizeof(prev_hex));
  bytes_to_hex(block.hmac, KOLIBRI_HASH_SIZE, hmac_hex, sizeof(hmac_hex));

  int written = fprintf(ctx->file, "%llu,%llu,%s,%s,%s,%s\n",
                        (unsigned long long)block.index,
                        (unsigned long long)block.timestamp, prev_hex, hmac_hex,
                        block.event_type, block.payload);
  if (written < 0) {
    return -1;
  }

  fflush(ctx->file);

  if (out_block) {
    *out_block = block;
  }

  return 0;
}

int kg_verify_file(const char *path, const unsigned char *key,
                   size_t key_len) {
  if (!path || !key || key_len == 0 || key_len > KOLIBRI_HMAC_KEY_SIZE) {
    return -1;
  }

  FILE *file = fopen(path, "rb");
  if (!file) {
    if (errno == ENOENT) {
      return 1;
    }
    return -1;
  }

  unsigned char expected_prev[KOLIBRI_HASH_SIZE];
  memset(expected_prev, 0, sizeof(expected_prev));
  uint64_t expected_index = 0;

  char line[1024];
  while (fgets(line, sizeof(line), file)) {
    ReasonBlock block;
    memset(&block, 0, sizeof(block));

    char prev_hash_hex[KOLIBRI_HASH_SIZE * 2 + 1];
    char hmac_hex[KOLIBRI_HASH_SIZE * 2 + 1];
    char event[KOLIBRI_EVENT_TYPE_SIZE];
    char payload[KOLIBRI_PAYLOAD_SIZE];
    unsigned long long index = 0ULL;
    unsigned long long timestamp = 0ULL;

    int matched = sscanf(line, "%llu,%llu,%64[^,],%64[^,],%31[^,],%255[^\n]",
                         &index, &timestamp, prev_hash_hex, hmac_hex, event,
                         payload);
    if (matched != 6) {
      fclose(file);
      return -1;
    }

    block.index = (uint64_t)index;
    block.timestamp = (uint64_t)timestamp;
    strncpy(block.event_type, event, sizeof(block.event_type) - 1);
    strncpy(block.payload, payload, sizeof(block.payload) - 1);

    if (block.index != expected_index) {
      fclose(file);
      return -1;
    }

    if (hex_to_bytes(prev_hash_hex, block.prev_hash, KOLIBRI_HASH_SIZE) != 0 ||
        hex_to_bytes(hmac_hex, block.hmac, KOLIBRI_HASH_SIZE) != 0) {
      fclose(file);
      return -1;
    }

    if (memcmp(block.prev_hash, expected_prev, KOLIBRI_HASH_SIZE) != 0) {
      fclose(file);
      return -1;
    }

    unsigned char buffer[sizeof(block.index) + sizeof(block.timestamp) +
                         KOLIBRI_HASH_SIZE + sizeof(block.event_type) +
                         sizeof(block.payload)];
    size_t buffer_len = 0;
    build_payload_buffer(&block, buffer, &buffer_len);

    unsigned char computed[KOLIBRI_HASH_SIZE];
    unsigned int hmac_len = 0;
    unsigned char *result = HMAC(EVP_sha256(), key, (int)key_len, buffer,
                                 buffer_len, computed, &hmac_len);
    if (!result || hmac_len != KOLIBRI_HASH_SIZE) {
      fclose(file);
      return -1;
    }

    if (memcmp(computed, block.hmac, KOLIBRI_HASH_SIZE) != 0) {
      fclose(file);
      return -1;
    }

    memcpy(expected_prev, block.hmac, KOLIBRI_HASH_SIZE);
    expected_index = block.index + 1;
  }

  fclose(file);
  return 0;
}
