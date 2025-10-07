#define _GNU_SOURCE

#include "kolibri/formula.h"
#include "kolibri/script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KolibriFormulaPool g_pool;
static KolibriScript g_script;
static int g_bridge_ready = 0;

static int bridge_ensure_initialized(void) {
    if (g_bridge_ready) {
        return 0;
    }

    kf_pool_init(&g_pool, 424242ULL);
    if (ks_init(&g_script, &g_pool, NULL) != 0) {
        return -1;
    }

    g_bridge_ready = 1;
    return 0;
}

int kolibri_bridge_init(void) {
    g_bridge_ready = 0;
    return bridge_ensure_initialized();
}

int kolibri_bridge_reset(void) {
    if (g_bridge_ready) {
        ks_free(&g_script);
        g_bridge_ready = 0;
    }
    return bridge_ensure_initialized();
}

int kolibri_bridge_execute(const char *program_utf8, char *out_buffer, size_t out_capacity) {
    if (!program_utf8 || !out_buffer || out_capacity == 0) {
        return -5;
    }

    if (bridge_ensure_initialized() != 0) {
        out_buffer[0] = '\0';
        return -1;
    }

    FILE *sink = NULL;
#if defined(__EMSCRIPTEN__)
    char *sink_buffer = NULL;
    size_t sink_size = 0U;
    sink = open_memstream(&sink_buffer, &sink_size);
#else
    sink = tmpfile();
#endif
    if (!sink) {
        out_buffer[0] = '\0';
        return -2;
    }

    ks_set_output(&g_script, sink);
    if (ks_load_text(&g_script, program_utf8) != 0) {
        fclose(sink);
#if defined(__EMSCRIPTEN__)
        free(sink_buffer);
#endif
        ks_set_output(&g_script, stdout);
        out_buffer[0] = '\0';
        return -3;
    }

    if (ks_execute(&g_script) != 0) {
        fclose(sink);
#if defined(__EMSCRIPTEN__)
        free(sink_buffer);
#endif
        ks_set_output(&g_script, stdout);
        out_buffer[0] = '\0';
        return -4;
    }

    fflush(sink);
#if defined(__EMSCRIPTEN__)
    fclose(sink);
    ks_set_output(&g_script, stdout);

    size_t copy = sink_size < (out_capacity - 1U) ? sink_size : (out_capacity - 1U);
    if (copy > 0U) {
        memcpy(out_buffer, sink_buffer, copy);
    }
    out_buffer[copy] = '\0';
    free(sink_buffer);

    return (int)copy;
#else
    if (fseek(sink, 0L, SEEK_SET) != 0) {
        fclose(sink);
        ks_set_output(&g_script, stdout);
        out_buffer[0] = '\0';
        return -2;
    }

    size_t written = fread(out_buffer, 1U, out_capacity - 1U, sink);
    out_buffer[written] = '\0';

    fclose(sink);
    ks_set_output(&g_script, stdout);

    return (int)written;
#endif
}
