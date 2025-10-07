#include "kolibri/genome.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void test_genome(void) {
  char template[] = "/tmp/kolibri_genomeXXXXXX";
  int fd = mkstemp(template);
  assert(fd != -1);
  close(fd);

  KolibriGenome genome;
  const unsigned char key[] = "test-key";
  int rc = kg_open(&genome, template, key, sizeof(key) - 1);
  assert(rc == 0);

  ReasonBlock block;
  rc = kg_append(&genome, "TEST", "payload", &block);
  assert(rc == 0);
  assert(block.index == 0);

  kg_close(&genome);

  rc = kg_verify_file(template, key, sizeof(key) - 1);
  assert(rc == 0);

  FILE *f = fopen(template, "r+");
  assert(f != NULL);
  int ch = fgetc(f);
  assert(ch != EOF);
  fseek(f, 0, SEEK_SET);
  fputc(ch == '0' ? '1' : '0', f);
  fclose(f);

  rc = kg_verify_file(template, key, sizeof(key) - 1);
  assert(rc == -1);

  remove(template);

  rc = kg_verify_file(template, key, sizeof(key) - 1);
  assert(rc == 1);
}
