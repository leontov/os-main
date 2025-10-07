#include <stdint.h>
#include <stddef.h>

static volatile uint16_t *const VGA_BUFFER = (uint16_t *)0xB8000;
static const uint8_t VGA_WIDTH = 80;
static const uint8_t VGA_HEIGHT = 25;
static uint8_t cursor_row = 0;
static uint8_t cursor_col = 0;
static char command_buffer[128];
static uint8_t command_length = 0;
static uint8_t history_count = 0;
static uint8_t history_head = 0;
static uint8_t history_cursor = 0;
static char history_entries[16][128];
static int expect_extended = 0;

static void vga_clear(void) {
    uint16_t entry = (uint16_t)(' ' | (0x07 << 8));
    for (uint32_t i = 0; i < (uint32_t)VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA_BUFFER[i] = entry;
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void vga_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        if (++cursor_row >= VGA_HEIGHT) {
            cursor_row = 0;
        }
        return;
    }
    uint16_t entry = (uint16_t)(c | (0x07 << 8));
    VGA_BUFFER[(uint32_t)cursor_row * VGA_WIDTH + cursor_col] = entry;
    if (++cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        if (++cursor_row >= VGA_HEIGHT) {
            cursor_row = 0;
        }
    }
}

static void vga_backspace(void) {
    if (cursor_col == 0) {
        if (cursor_row == 0) {
            return;
        }
        cursor_row--;
        cursor_col = VGA_WIDTH - 1;
    } else {
        cursor_col--;
    }
    uint16_t entry = (uint16_t)(' ' | (0x07 << 8));
    VGA_BUFFER[(uint32_t)cursor_row * VGA_WIDTH + cursor_col] = entry;
}

static void vga_print(const char *str) {
    while (*str) {
        vga_putc(*str++);
    }
}

static void vga_prompt(void) {
    vga_print("kolibri> ");
}

static void vga_draw_banner(void) {
    static const char *logo[] = {
        "  _  __     _ _ _ _          ",
        " | |/ /__ _(_) (_) |__  _ __  ",
        " | ' // _` | | | | '_ \\| '_ \\",
        " | . \\ (_| | | | | |_) | | | |",
        " |_|\\_\\__,_|_|_|_|_.__/|_| |_|",
        "",
        " Kolibri OS :: χ→Φ→S prototype"
    };

    vga_clear();
    uint8_t row = 0;
    for (int line = 0; line < (int)(sizeof(logo) / sizeof(logo[0])); ++line) {
        const char *text = logo[line];
        cursor_row = row++;
        cursor_col = 0;
        while (*text) {
            vga_putc(*text++);
        }
    }
    cursor_row = row + 1;
    cursor_col = 0;
    vga_print("Shell commands:\n");
    vga_print("  help      - show help\n");
    vga_print("  about     - Kolibri summary\n");
    vga_print("  clear     - clear screen\n");
    vga_print("  halt      - hang CPU\n");
    vga_print("  kolibri   - χ→Φ→S step\n");
    vga_print("  history   - show recent steps\n");
    vga_print("  chain     - list Kolibri Chain (save/load)\n");
    vga_print("  verify    - verify chain integrity\n\n");
}

static void print_banner(void) {
    vga_draw_banner();
    vga_prompt();
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int keyboard_has_data(void) {
    return (inb(0x64) & 0x01) != 0;
}

static char translate_scancode(uint8_t scancode) {
    static const char map[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' ',
    };
    if (scancode & 0x80) {
        return 0; // key release
    }
    if (scancode < sizeof(map)) {
        return map[scancode];
    }
    return 0;
}

static int str_equals(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++) {
            return 0;
        }
    }
    return *a == 0 && *b == 0;
}

static void execute_command(const char *cmd);

static uint32_t kolibri_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float kolibri_norm(uint32_t x) {
    return (float)(x & 0xFFFFFF) / (float)0xFFFFFF;
}

static float kolibri_chi(uint32_t q) {
    uint32_t seed = kolibri_hash(q ^ 0xD1B54A32U);
    float u = kolibri_norm(seed);
    float t = 1.0f - (float)((u * 2.0f) > 1.0f ? (u * 2.0f) - 1.0f : 1.0f - u * 2.0f);
    float l = 4.0f * t * (1.0f - t);
    return (t + l) * 0.5f;
}

static float kolibri_phi(float chi) {
    float z = 2.0f * chi - 1.0f;
    float t1 = z;
    float t2 = 2.0f * z * z - 1.0f;
    return 0.8f * chi + 0.15f * t1 + 0.05f * t2;
}

static float kolibri_score(uint32_t q, float value) {
    float target = kolibri_norm(q);
    float diff = value - target;
    if (diff < 0.0f) {
        diff = -diff;
    }
    return -diff;
}

typedef struct {
    uint32_t q;
    float chi;
    float phi;
    float score;
} KolibriTrace;

#define TRACE_CAPACITY 8
static KolibriTrace trace_buffer[TRACE_CAPACITY];
static uint8_t trace_index = 0;

static void trace_add(uint32_t q, float chi, float phi, float score) {
    trace_buffer[trace_index % TRACE_CAPACITY] = (KolibriTrace){q, chi, phi, score};
    trace_index++;
}

static void kolibri_history(void);
static void chain_append(const char *tag, uint32_t q, float score, const char *payload);
static void chain_list(void);
static void chain_verify(void);
static void chain_save(void);
static void chain_load(void);

/* === χ→Φ→S engine ===================================================== */

static uint64_t kolibri_splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float kolibri_wrap_pi(float x) {
    const float PI = 3.1415926f;
    const float TWO_PI = 6.2831852f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    return x;
}

static float kolibri_sin(float x) {
    x = kolibri_wrap_pi(x);
    float x2 = x * x;
    return x - (x2 * x) / 6.0f + (x2 * x2 * x) / 120.0f;
}

static float kolibri_clamp01(float x) {
    if (x < 1e-6f) return 1e-6f;
    if (x > 1.0f - 1e-6f) return 1.0f - 1e-6f;
    return x;
}

static float kolibri_chi64(uint64_t seed) {
    uint64_t a = kolibri_splitmix64(seed);
    uint64_t b = kolibri_splitmix64(a ^ 0xD1B54A32D192ED03ULL);
    float u = (float)((b >> 11) | 1ULL) / (float)(1ULL << 53);
    float t = 1.0f - (float)((u * 2.0f) > 1.0f ? (u * 2.0f) - 1.0f : 1.0f - u * 2.0f);
    float l = 4.0f * t * (1.0f - t);
    return kolibri_clamp01((t + l) * 0.5f);
}

static float kolibri_phi64(float x) {
    x = kolibri_clamp01(x);
    float z = 2.0f * x - 1.0f;
    float t1 = z;
    float t2 = 2.0f * z * z - 1.0f;
    float s1 = kolibri_sin(3.1415926f * x);
    float s2 = kolibri_sin(2.0f * 3.1415926f * x);
    return 0.6f * x + 0.25f * t1 + 0.1f * t2 + 0.05f * s2 + 0.05f * s1;
}

static float kolibri_score64(uint64_t q, float value) {
    float target = (float)((q >> 11) | 1ULL) / (float)(1ULL << 53);
    float diff = value - target;
    if (diff < 0.0f) diff = -diff;
    return -diff;
}

typedef struct {
    uint64_t identifier;
    float chi;
    float phi;
    float score;
} KolibriNode;

static void kolibri_run(uint64_t q, float *chi_out, float *phi_out, float *score_out) {
    const int BEAM = 8;
    const int DEPTH = 4;
    KolibriNode current[BEAM];
    KolibriNode next[BEAM];
    int count = 0;
    uint64_t seed_base = 0xD1B54A32D192ED03ULL;

    for (int d = 0; d < 16 && count < BEAM; ++d) {
        uint64_t id = kolibri_splitmix64(seed_base ^ q ^ (uint64_t)d);
        float chi = kolibri_chi64(id);
        float phi = kolibri_phi64(chi);
        float score = kolibri_score64(q, phi);
        current[count++] = (KolibriNode){id, chi, phi, score};
    }
    int cur_size = count;

    for (int level = 1; level < DEPTH; ++level) {
        int next_size = 0;
        for (int i = 0; i < cur_size && next_size < BEAM; ++i) {
            uint64_t base = kolibri_splitmix64(current[i].identifier ^ seed_base ^ (uint64_t)(level * 0x9E37));
            for (int d = 0; d < 8 && next_size < BEAM; ++d) {
                uint64_t id = kolibri_splitmix64(base ^ (uint64_t)d);
                float chi = kolibri_chi64(id);
                float phi = kolibri_phi64(chi);
                float score = kolibri_score64(q, phi);
                next[next_size++] = (KolibriNode){id, chi, phi, score};
            }
        }
        if (next_size == 0) {
            break;
        }
        // selection sort by score
        for (int i = 0; i < next_size - 1; ++i) {
            int best = i;
            for (int j = i + 1; j < next_size; ++j) {
                if (next[j].score > next[best].score) best = j;
            }
            if (best != i) {
                KolibriNode tmp = next[i];
                next[i] = next[best];
                next[best] = tmp;
            }
        }
        cur_size = next_size < BEAM ? next_size : BEAM;
        for (int i = 0; i < cur_size; ++i) {
            current[i] = next[i];
        }
    }

    KolibriNode best = current[0];
    for (int i = 1; i < cur_size; ++i) {
        if (current[i].score > best.score) {
            best = current[i];
        }
    }
    *chi_out = best.chi;
    *phi_out = best.phi;
    *score_out = best.score;
}

static void print_uint(uint32_t value) {
    char digits[16];
    int pos = 0;
    if (value == 0) {
        vga_putc('0');
        return;
    }
    while (value && pos < (int)sizeof(digits)) {
        digits[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (pos--) {
        vga_putc(digits[pos]);
    }
}

static void print_fixed(float value) {
    if (value < 0.0f) {
        vga_putc('-');
        value = -value;
    }
    uint32_t integer = (uint32_t)value;
    uint32_t fraction = (uint32_t)((value - (float)integer) * 1000.0f + 0.5f);
    print_uint(integer);
    vga_putc('.');
    if (fraction < 100) vga_putc('0');
    if (fraction < 10) vga_putc('0');
    print_uint(fraction);
}

static void kolibri_step(const char *args) {
    while (*args == ' ') {
        ++args;
    }
    if (!*args) {
        vga_print("usage: kolibri <q>\n");
        return;
    }
    uint32_t q = 0;
    while (*args) {
        if (*args < '0' || *args > '9') {
            vga_print("invalid number\n");
            return;
        }
        q = q * 10U + (uint32_t)(*args - '0');
        ++args;
    }
    float chi = kolibri_chi(q);
    float phi = kolibri_phi(chi);
    float score = kolibri_score(q, phi);
    trace_add(q, chi, phi, score);
    chain_append("kolibri", q, score, "step");

    vga_print("q=");
    print_uint(q);
    vga_print(" chi=");
    print_fixed(chi);
    vga_print(" phi=");
    print_fixed(phi);
    vga_print(" score=");
    print_fixed(score);
    vga_putc('\n');
}

static void kolibri_history(void) {
    if (trace_index == 0) {
        vga_print("trace empty\n");
        return;
    }
    uint8_t start = trace_index > TRACE_CAPACITY ? trace_index - TRACE_CAPACITY : 0;
    for (uint8_t i = start; i < trace_index; ++i) {
        KolibriTrace entry = trace_buffer[i % TRACE_CAPACITY];
        vga_putc('[');
        print_uint(i);
        vga_print("] q=");
        print_uint(entry.q);
        vga_print(" chi=");
        print_fixed(entry.chi);
        vga_print(" phi=");
        print_fixed(entry.phi);
        vga_print(" score=");
        print_fixed(entry.score);
        vga_putc('\n');
    }
}

/* === Kolibri Chain ===================================================== */

typedef struct {
    uint32_t index;
    uint32_t prev_hash;
    uint32_t hash;
    uint32_t q;
    float score;
    char tag[16];
    char payload[64];
} KolibriChainEntry;

#define CHAIN_CAPACITY 32
static KolibriChainEntry chain_entries[CHAIN_CAPACITY];
static uint32_t chain_size = 0;
static uint32_t chain_key = 0xA5B3571DU;
static KolibriChainEntry chain_snapshot[CHAIN_CAPACITY];
static uint32_t chain_snapshot_size = 0;
static uint8_t chain_snapshot_valid = 0;

static uint32_t chain_hash(const KolibriChainEntry *entry) {
    uint32_t h = chain_key;
    const uint8_t *bytes = (const uint8_t *)entry->payload;
    for (uint32_t i = 0; i < sizeof(entry->payload); ++i) {
        h ^= bytes[i];
        h *= 16777619U;
    }
    h ^= entry->q;
    h ^= (uint32_t)(entry->score * 100000.0f);
    h ^= entry->prev_hash;
    return h;
}

static void chain_append(const char *tag, uint32_t q, float score, const char *payload) {
    KolibriChainEntry entry;
    entry.index = chain_size;
    entry.prev_hash = chain_size ? chain_entries[(chain_size - 1) % CHAIN_CAPACITY].hash : 0xFACEFEEDU;
    entry.q = q;
    entry.score = score;
    for (int i = 0; i < 15; ++i) {
        entry.tag[i] = tag && tag[i] ? tag[i] : '\0';
        if (!tag || !tag[i]) break;
    }
    entry.tag[15] = '\0';
    for (int i = 0; i < 63; ++i) {
        entry.payload[i] = payload && payload[i] ? payload[i] : '\0';
        if (!payload || !payload[i]) break;
    }
    entry.payload[63] = '\0';
    entry.hash = chain_hash(&entry);
    chain_entries[chain_size % CHAIN_CAPACITY] = entry;
    chain_size++;
}

static void chain_list(void) {
    if (chain_size == 0) {
        vga_print("chain empty\n");
        return;
    }
    uint32_t start = chain_size > CHAIN_CAPACITY ? chain_size - CHAIN_CAPACITY : 0;
    for (uint32_t i = start; i < chain_size; ++i) {
        KolibriChainEntry entry = chain_entries[i % CHAIN_CAPACITY];
        vga_putc('#');
        print_uint(entry.index);
        vga_print(" q=");
        print_uint(entry.q);
        vga_print(" score=");
        print_fixed(entry.score);
        vga_print(" tag=");
        vga_print(entry.tag);
        vga_print(" hash=");
        print_uint(entry.hash);
        vga_putc('\n');
    }
}

static void chain_verify(void) {
    if (chain_size == 0) {
        vga_print("chain empty\n");
        return;
    }
    uint32_t start = chain_size > CHAIN_CAPACITY ? chain_size - CHAIN_CAPACITY : 0;
    for (uint32_t i = start; i < chain_size; ++i) {
        KolibriChainEntry entry = chain_entries[i % CHAIN_CAPACITY];
        uint32_t expected = chain_hash(&entry);
        if (expected != entry.hash) {
            vga_print("chain corrupted at #");
            print_uint(entry.index);
            vga_putc('\n');
            return;
        }
        if (i > start) {
            uint32_t prev_hash = chain_entries[(i - 1) % CHAIN_CAPACITY].hash;
            if (entry.prev_hash != prev_hash) {
                vga_print("chain break at #");
                print_uint(entry.index);
                vga_putc('\n');
                return;
            }
        }
    }
    vga_print("chain ok\n");
}

static void chain_save(void) {
    for (uint32_t i = 0; i < (chain_size < CHAIN_CAPACITY ? chain_size : CHAIN_CAPACITY); ++i) {
        chain_snapshot[i] = chain_entries[i];
    }
    chain_snapshot_size = chain_size;
    chain_snapshot_valid = 1;
    vga_print("chain saved\n");
}

static void chain_load(void) {
    if (!chain_snapshot_valid) {
        vga_print("no snapshot\n");
        return;
    }
    uint32_t limit = chain_snapshot_size < CHAIN_CAPACITY ? chain_snapshot_size : CHAIN_CAPACITY;
    for (uint32_t i = 0; i < limit; ++i) {
        chain_entries[i] = chain_snapshot[i];
    }
    chain_size = chain_snapshot_size;
    vga_print("chain restored\n");
}

static void execute_command(const char *cmd) {
    if (str_equals(cmd, "help")) {
        vga_print("Commands: help, about, clear, halt, kolibri, history, chain, verify\n");
    } else if (str_equals(cmd, "about")) {
        vga_print("Kolibri OS :: χ→Φ→S prototype running bare metal.\n");
    } else if (str_equals(cmd, "clear")) {
        vga_clear();
        print_banner();
        return;
    } else if (str_equals(cmd, "halt")) {
        vga_print("Halting CPU...\n");
        for (;;) {
            __asm__ volatile("hlt");
        }
    } else if (str_equals(cmd, "history")) {
        kolibri_history();
    } else if (cmd[0] == 'c' && cmd[1] == 'h' && cmd[2] == 'a' && cmd[3] == 'i' && cmd[4] == 'n') {
        const char *arg = cmd + 5;
        while (*arg == ' ') ++arg;
        if (*arg == '\0') {
            chain_list();
        } else if (str_equals(arg, "save")) {
            chain_save();
        } else if (str_equals(arg, "load")) {
            chain_load();
        } else {
            vga_print("usage: chain [save|load]\n");
        }
    } else if (str_equals(cmd, "verify")) {
        chain_verify();
    } else if (*cmd) {
        if (cmd[0] == 'k' && cmd[1] == 'o' && cmd[2] == 'l' && cmd[3] == 'i' &&
            cmd[4] == 'b' && cmd[5] == 'r' && cmd[6] == 'i') {
            kolibri_step(cmd + 7);
        } else {
            vga_print("Unknown command. Type 'help'.\n");
        }
    }
}

static void handle_input(void) {
    uint8_t scancode = inb(0x60);
    if (scancode == 0xE0) {
        expect_extended = 1;
        return;
    }
    if (expect_extended) {
        if (scancode == 0x48) { // Up arrow
            if (history_count) {
                if (history_cursor > 0) {
                    history_cursor--;
                }
                uint8_t index = (history_head + history_cursor) % 16;
                while (command_length > 0) {
                    command_length--;
                    vga_backspace();
                }
                const char *cmd = history_entries[index];
                for (uint8_t i = 0; cmd[i] && i < sizeof(command_buffer) - 1; ++i) {
                    command_buffer[i] = cmd[i];
                    vga_putc(cmd[i]);
                    command_length = i + 1;
                }
                command_buffer[command_length] = '\0';
            }
        } else if (scancode == 0x50) { // Down arrow
            if (history_count) {
                if (history_cursor + 1 < history_count) {
                    history_cursor++;
                    uint8_t index = (history_head + history_cursor) % 16;
                    while (command_length > 0) {
                        command_length--;
                        vga_backspace();
                    }
                    const char *cmd = history_entries[index];
                    for (uint8_t i = 0; cmd[i] && i < sizeof(command_buffer) - 1; ++i) {
                        command_buffer[i] = cmd[i];
                        vga_putc(cmd[i]);
                        command_length = i + 1;
                    }
                    command_buffer[command_length] = '\0';
                } else {
                    while (command_length > 0) {
                        command_length--;
                        vga_backspace();
                    }
                    command_buffer[0] = '\0';
                    history_cursor = history_count;
                }
            }
        }
        expect_extended = 0;
        return;
    }
    char ch = translate_scancode(scancode);
    if (!ch) {
        return;
    }
    if (ch == '\b') {
        if (command_length > 0) {
            command_length--;
            command_buffer[command_length] = '\0';
            vga_backspace();
        }
        return;
    }
    if (ch == '\n') {
        vga_putc('\n');
        command_buffer[command_length] = '\0';
        if (command_length > 0) {
            uint8_t slot = (history_head + history_count) % 16;
            for (uint8_t i = 0; i < command_length && i < sizeof(history_entries[0]) - 1; ++i) {
                history_entries[slot][i] = command_buffer[i];
            }
            history_entries[slot][command_length] = '\0';
            if (history_count < 16) {
                history_count++;
            } else {
                history_head = (history_head + 1) % 16;
            }
        }
        execute_command(command_buffer);
        command_length = 0;
        command_buffer[0] = '\0';
        history_cursor = history_count;
        vga_prompt();
        return;
    }
    if (command_length < sizeof(command_buffer) - 1 && ch >= 32) {
        command_buffer[command_length++] = ch;
        command_buffer[command_length] = '\0';
        vga_putc(ch);
    }
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_magic;
    (void)multiboot_info;
    vga_clear();
    print_banner();

    for (;;) {
        if (keyboard_has_data()) {
            handle_input();
        }
    }
}
