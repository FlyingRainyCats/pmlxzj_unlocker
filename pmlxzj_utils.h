#pragma once

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define PMLXZJ_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define PMLXZJ_UNUSED_PARAMETER(x) (void)(x)

static inline void pmlxzj_util_hexdump(void* ptr, int buflen) {
  unsigned char* buf = (unsigned char*)ptr;
  int i, j;
  for (i = 0; i < buflen; i += 16) {
    printf("%06x: ", i);
    for (j = 0; j < 16; j++)
      if (i + j < buflen)
        printf("%02x ", buf[i + j]);
      else
        printf("   ");
    printf(" ");
    for (j = 0; j < 16; j++)
      if (i + j < buflen)
        printf("%c", isprint(buf[i + j]) ? buf[i + j] : '.');
    printf("\n");
  }
}

static inline void pmlxzj_util_copy_file(FILE* f_dst, FILE* f_src) {
  fseek(f_src, 0, SEEK_END);
  size_t bytes_left = (size_t)ftell(f_src);
  fseek(f_src, 0, SEEK_SET);

  char* buffer = malloc(4096);
  size_t bytes_read;
  do {
    bytes_read = fread(buffer, 1, PMLXZJ_MIN(4096, bytes_left), f_src);
    bytes_left -= bytes_read;
    fwrite(buffer, 1, bytes_read, f_dst);
  } while (bytes_read != 0);

  assert(bytes_left == 0 && "not all bytes read");
  free(buffer);
  buffer = NULL;
}
