#include "kolibri/decimal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_transducer_roundtrip(void) {
  const unsigned char payload[] = {0u, 1u, 2u, 10u, 99u, 128u, 255u};
  uint8_t buffer[64];
  k_digit_stream stream;
  k_digit_stream_init(&stream, buffer, sizeof(buffer));
  int rc = k_transduce_utf8(&stream, payload, sizeof(payload));
  assert(rc == 0);
  unsigned char restored[16];
  size_t written = 0;
  rc = k_emit_utf8(&stream, restored, sizeof(restored), &written);
  assert(rc == 0);
  assert(written == sizeof(payload));
  assert(memcmp(payload, restored, sizeof(payload)) == 0);
}

static void test_digit_stream_bounds(void) {
  uint8_t buffer[3];
  k_digit_stream stream;
  k_digit_stream_init(&stream, buffer, sizeof(buffer));
  assert(k_digit_stream_push(&stream, 1) == 0);
  assert(k_digit_stream_push(&stream, 9) == 0);
  assert(k_digit_stream_push(&stream, 5) == 0);
  assert(k_digit_stream_push(&stream, 2) != 0);
  k_digit_stream_rewind(&stream);
  uint8_t digit = 0;
  assert(k_digit_stream_read(&stream, &digit) == 0 && digit == 1);
  assert(k_digit_stream_read(&stream, &digit) == 0 && digit == 9);
  assert(k_digit_stream_read(&stream, &digit) == 0 && digit == 5);
  assert(k_digit_stream_read(&stream, &digit) == 1);
}

static void test_text_roundtrip(void) {
  const char *text = "Kolibri";
  char encoded[64];
  char decoded[32];
  assert(k_encode_text(text, encoded, sizeof(encoded)) == 0);
  assert(k_decode_text(encoded, decoded, sizeof(decoded)) == 0);
  assert(strcmp(text, decoded) == 0);
}

void test_decimal(void) {
  test_transducer_roundtrip();
  test_digit_stream_bounds();
  test_text_roundtrip();
}
