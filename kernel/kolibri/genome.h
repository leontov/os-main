#ifndef KOLIBRI_KERNEL_GENOME_H
#define KOLIBRI_KERNEL_GENOME_H

#include <stddef.h>
#include <stdint.h>

#define KOLIBRI_GENOME_CAPACITY 4096U
#define KOLIBRI_EVENT_TYPE_SIZE 32U
#define KOLIBRI_PAYLOAD_SIZE 128U

typedef struct {
    uint32_t index;
    char event_type[KOLIBRI_EVENT_TYPE_SIZE];
    char payload[KOLIBRI_PAYLOAD_SIZE];
} ReasonBlock;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    uint32_t next_index;
} KolibriGenome;

int kg_open(KolibriGenome *ctx, uint8_t *storage, size_t capacity);
void kg_close(KolibriGenome *ctx);
int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload, ReasonBlock *out_block);
int kg_replay(const KolibriGenome *ctx, ReasonBlock *out_block, uint32_t index);

#endif /* KOLIBRI_KERNEL_GENOME_H */
