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

static inline void pmlxzj_skip_lpe_data(FILE* file) {
  uint32_t len;
  fread(&len, sizeof(len), 1, file);
  fseek(file, (long)len, SEEK_CUR);
}

static inline void pmlxzj_util_copy(FILE* f_dst, FILE* f_src, size_t len) {
  char* buffer = malloc(4096);
  size_t bytes_read;
  while(len > 0) {
    bytes_read = fread(buffer, 1, PMLXZJ_MIN(4096, len), f_src);
    if (bytes_read == 0) {
      break;
    }
    len -= bytes_read;
    fwrite(buffer, 1, bytes_read, f_dst);
  }
  assert(len == 0);
  free(buffer);
  buffer = NULL;
}

static inline void pmlxzj_util_copy_file(FILE* f_dst, FILE* f_src) {
  fseek(f_src, 0, SEEK_END);
  size_t file_size = (size_t)ftell(f_src);

  fseek(f_src, 0, SEEK_SET);
  pmlxzj_util_copy(f_dst, f_src, file_size);
}
