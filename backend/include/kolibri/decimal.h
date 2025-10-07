#ifndef KOLIBRI_DECIMAL_H
#define KOLIBRI_DECIMAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    uint8_t *digits;
    size_t capacity;
    size_t length;
    size_t cursor;
} k_digit_stream;

void k_digit_stream_init(k_digit_stream *stream, uint8_t *buffer, size_t capacity);
void k_digit_stream_reset(k_digit_stream *stream);
void k_digit_stream_rewind(k_digit_stream *stream);
int k_digit_stream_push(k_digit_stream *stream, uint8_t digit);
int k_digit_stream_read(k_digit_stream *stream, uint8_t *digit);
size_t k_digit_stream_remaining(const k_digit_stream *stream);

int k_transduce_utf8(k_digit_stream *stream, const unsigned char *bytes, size_t len);
int k_emit_utf8(const k_digit_stream *stream, unsigned char *out, size_t out_len, size_t *written);


size_t k_encode_text_length(size_t input_len);
size_t k_decode_text_length(size_t digits_len);
int k_encode_text(const char *input, char *out, size_t out_len);
int k_decode_text(const char *digits, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* KOLIBRI_DECIMAL_H */
