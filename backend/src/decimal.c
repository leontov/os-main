/*
 * Copyright (c) 2025 Кочуров Владислав Евгеньевич
 */

#include "kolibri/decimal.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

static int ensure_space(k_digit_stream *stream) {
    if (!stream || !stream->digits) {
        return -1;
    }
    if (stream->length >= stream->capacity) {
        return -1;
    }
    return 0;
}

void k_digit_stream_init(k_digit_stream *stream, uint8_t *buffer, size_t capacity) {
    if (!stream) {
        return;
    }
    stream->digits = buffer;
    stream->capacity = capacity;
    stream->length = 0;
    stream->cursor = 0;
    if (stream->digits && stream->capacity > 0) {
        memset(stream->digits, 0, stream->capacity);
    }
}

void k_digit_stream_reset(k_digit_stream *stream) {
    if (!stream || !stream->digits) {
        return;
    }
    memset(stream->digits, 0, stream->capacity);
    stream->length = 0;
    stream->cursor = 0;
}

void k_digit_stream_rewind(k_digit_stream *stream) {
    if (!stream) {
        return;
    }
    stream->cursor = 0;
}

int k_digit_stream_push(k_digit_stream *stream, uint8_t digit) {
    if (digit > 9) {
        return -1;
    }
    if (ensure_space(stream) != 0) {
        return -1;
    }
    stream->digits[stream->length++] = digit;
    return 0;
}

int k_digit_stream_read(k_digit_stream *stream, uint8_t *digit) {
    if (!stream || !digit) {
        return -1;
    }
    if (stream->cursor >= stream->length) {
        return 1;
    }
    *digit = stream->digits[stream->cursor++];
    return 0;
}

size_t k_digit_stream_remaining(const k_digit_stream *stream) {
    if (!stream) {
        return 0;
    }
    if (stream->cursor >= stream->length) {
        return 0;
    }
    return stream->length - stream->cursor;
}

static int encode_byte(k_digit_stream *stream, unsigned char value) {
    uint8_t hundreds = (uint8_t)(value / 100U);
    uint8_t tens = (uint8_t)((value / 10U) % 10U);
    uint8_t ones = (uint8_t)(value % 10U);
    if (k_digit_stream_push(stream, hundreds) != 0 ||
        k_digit_stream_push(stream, tens) != 0 ||
        k_digit_stream_push(stream, ones) != 0) {
        return -1;
    }
    return 0;
}

int k_transduce_utf8(k_digit_stream *stream, const unsigned char *bytes, size_t len) {
    if (!stream || !bytes) {
        return -1;
    }
    for (size_t i = 0; i < len; ++i) {
        if (encode_byte(stream, bytes[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

int k_emit_utf8(const k_digit_stream *stream, unsigned char *out, size_t out_len,
                size_t *written) {
    if (!stream || !out) {
        return -1;
    }
    if (stream->length % 3 != 0) {
        return -1;
    }
    size_t expected = stream->length / 3;
    if (out_len < expected) {
        return -1;
    }
    for (size_t i = 0; i < expected; ++i) {
        size_t offset = i * 3U;
        unsigned int value = (unsigned int)(stream->digits[offset] * 100U +
                                            stream->digits[offset + 1] * 10U +
                                            stream->digits[offset + 2]);
        out[i] = (unsigned char)value;
    }
    if (written) {
        *written = expected;
    }
    return 0;
}

size_t k_encode_text_length(size_t input_len) {
    return input_len * 3 + 1;
}

size_t k_decode_text_length(size_t digits_len) {
    if (digits_len % 3 != 0) {
        return 0;
    }
    return digits_len / 3 + 1;
}

int k_encode_text(const char *input, char *out, size_t out_len) {
    if (!input || !out) {
        return -1;
    }
    size_t len = strlen(input);
    size_t need = len * 3U + 1U;
    if (out_len < need) {
        return -1;
    }
    uint8_t buffer[512];
    if (len * 3U > sizeof(buffer)) {
        return -1;
    }
    k_digit_stream stream;
    k_digit_stream_init(&stream, buffer, sizeof(buffer));
    if (k_transduce_utf8(&stream, (const unsigned char *)input, len) != 0) {
        return -1;
    }
    for (size_t i = 0; i < stream.length; ++i) {
        out[i] = (char)('0' + stream.digits[i]);
    }
    out[stream.length] = '\0';
    return 0;
}

int k_decode_text(const char *digits, char *out, size_t out_len) {
    if (!digits || !out) {
        return -1;
    }
    size_t len = strlen(digits);
    if (len % 3U != 0U) {
        return -1;
    }
    size_t expected = len / 3U;
    if (out_len < expected + 1U) {
        return -1;
    }
    uint8_t buffer[512];
    if (len > sizeof(buffer)) {
        return -1;
    }
    k_digit_stream stream;
    k_digit_stream_init(&stream, buffer, len);
    for (size_t i = 0; i < len; ++i) {
        if (digits[i] < '0' || digits[i] > '9') {
            return -1;
        }
        stream.digits[i] = (uint8_t)(digits[i] - '0');
    }
    stream.length = len;
    unsigned char decoded[256];
    size_t produced = 0;
    if (k_emit_utf8(&stream, decoded, sizeof(decoded), &produced) != 0) {
        return -1;
    }
    for (size_t i = 0; i < produced; ++i) {
        out[i] = (char)decoded[i];
    }
    out[produced] = '\0';
    return 0;
}
