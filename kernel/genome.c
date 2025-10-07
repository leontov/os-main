#include "kolibri/genome.h"

#include "support.h"

typedef struct {
    uint32_t index;
    char event_type[KOLIBRI_EVENT_TYPE_SIZE];
    char payload[KOLIBRI_PAYLOAD_SIZE];
} GenomeRecord;

int kg_open(KolibriGenome *ctx, uint8_t *storage, size_t capacity) {
    if (!ctx || !storage || capacity < sizeof(GenomeRecord)) {
        return -1;
    }
    ctx->data = storage;
    ctx->capacity = capacity;
    ctx->size = 0U;
    ctx->next_index = 0U;

    size_t offset = 0U;
    while (offset + sizeof(GenomeRecord) <= capacity) {
        const GenomeRecord *record = (const GenomeRecord *)(storage + offset);
        if (record->event_type[0] == '\0') {
            break;
        }
        ctx->size = offset + sizeof(GenomeRecord);
        ctx->next_index = record->index + 1U;
        offset += sizeof(GenomeRecord);
    }
    return 0;
}

void kg_close(KolibriGenome *ctx) {
    if (!ctx) {
        return;
    }
    ctx->data = NULL;
    ctx->size = 0U;
    ctx->capacity = 0U;
    ctx->next_index = 0U;
}

int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload, ReasonBlock *out_block) {
    if (!ctx || !ctx->data || !event_type || !payload) {
        return -1;
    }
    if (ctx->size + sizeof(GenomeRecord) > ctx->capacity) {
        return -1;
    }
    GenomeRecord *record = (GenomeRecord *)(ctx->data + ctx->size);
    k_memset(record, 0, sizeof(*record));
    record->index = ctx->next_index++;
    k_strlcpy(record->event_type, event_type, sizeof(record->event_type));
    k_strlcpy(record->payload, payload, sizeof(record->payload));
    ctx->size += sizeof(GenomeRecord);

    if (ctx->size + sizeof(GenomeRecord) <= ctx->capacity) {
        GenomeRecord *sentinel = (GenomeRecord *)(ctx->data + ctx->size);
        k_memset(sentinel, 0, sizeof(*sentinel));
    }

    if (out_block) {
        out_block->index = record->index;
        k_strlcpy(out_block->event_type, record->event_type, sizeof(out_block->event_type));
        k_strlcpy(out_block->payload, record->payload, sizeof(out_block->payload));
    }
    return 0;
}

int kg_replay(const KolibriGenome *ctx, ReasonBlock *out_block, uint32_t index) {
    if (!ctx || !ctx->data || !out_block) {
        return -1;
    }
    size_t offset = 0U;
    while (offset + sizeof(GenomeRecord) <= ctx->size) {
        const GenomeRecord *record = (const GenomeRecord *)(ctx->data + offset);
        if (record->index == index) {
            out_block->index = record->index;
            k_strlcpy(out_block->event_type, record->event_type, sizeof(out_block->event_type));
            k_strlcpy(out_block->payload, record->payload, sizeof(out_block->payload));
            return 0;
        }
        offset += sizeof(GenomeRecord);
    }
    return -1;
}
