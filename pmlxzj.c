#include "pmlxzj.h"
#include <memory.h>
#include <stdlib.h>
#include <string.h>

int scan_rows(uint32_t* dest_array, const char* str, size_t len) {
  const char* end = str + len - 1;

  uint32_t value = 0;
  int rows = 0;
  const char* p = str;
  while (p < end) {
    if (p[0] == '\r' && p[1] == '\n') {
      if (dest_array) {
        *dest_array++ = value;
      }
      p += 2;
      rows++;
      value = 0;
      continue;
    }
    value = value * 10 + (*p - '0');
    p++;
  }

  return rows;
}

pmlxzj_state_e scan_rows_to_u32_array(uint32_t** scanned_array, size_t* count, const char* str, size_t len) {
  *count = 0;
  int c = scan_rows(NULL, str, len);
  *scanned_array = calloc(c, sizeof(uint32_t));
  if (*scanned_array == NULL) {
    return PMLXZJ_ALLOCATE_INDEX_LIST_ERROR;
  }
  *count = c;
  scan_rows(*scanned_array, str, len);
  return PMLXZJ_OK;
}

pmlxzj_state_e scan_file_u32_array(uint32_t** scanned_array, size_t* count, FILE* f, long* offset) {
  long off = *offset - 4;
  fseek(f, off, SEEK_SET);
  uint32_t text_count = 0;
  fread(&text_count, sizeof(text_count), 1, f);
  off -= (long)text_count;
  char* text = malloc(text_count + 1);
  if (text == NULL) {
    return PMLXZJ_SCAN_U32_ARRAY_ALLOC_ERROR;
  }
  text[text_count] = 0;
  fseek(f, off, SEEK_SET);
  fread(text, 1, text_count, f);
  pmlxzj_state_e result = scan_rows_to_u32_array(scanned_array, count, text, text_count);
  free(text);
  if (result != PMLXZJ_OK) {
    *offset = 0;
    free(*scanned_array);
    *scanned_array = NULL;
  } else {
    *offset = off;
  }
  return result;
}

pmlxzj_state_e pmlxzj_init(pmlxzj_state_t* ctx, FILE* f_src) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->file = f_src;

  fseek(f_src, 0, SEEK_END);
  ctx->file_size = ftell(f_src);

  fseek(f_src, -(int)sizeof(ctx->footer), SEEK_END);
  fread(&ctx->footer, sizeof(ctx->footer), 1, f_src);

  if (strcmp(ctx->footer.magic, "pmlxzjtlx") != 0) {
    return PMLXZJ_MAGIC_NOT_MATCH;
  }

  if (ctx->footer.mode_1_nonce) {
    ctx->encrypt_mode = 1;
    snprintf(ctx->nonce_buffer, sizeof(ctx->nonce_buffer) - 1, "%d", ctx->footer.mode_1_nonce);
    for(int i = 0; i < 20; i++) {
      ctx->nonce_buffer_mode_1[i] = ctx->nonce_buffer[20 - i];
    }
  } else if (ctx->footer.mode_2_nonce) {
    ctx->encrypt_mode = 2;
    return PMLXZJ_UNSUPPORTED_MODE_2;
  } else {
    ctx->encrypt_mode = 0;
  }

  long offset = (long)(ctx->file_size - sizeof(ctx->footer));
  pmlxzj_state_e status = scan_file_u32_array(&ctx->idx1, &ctx->idx1_count, f_src, &offset);
  if (status != PMLXZJ_OK) {
    return status;
  }
  status = scan_file_u32_array(&ctx->idx2, &ctx->idx2_count, f_src, &offset);
  if (status != PMLXZJ_OK) {
    return status;
  }

  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_init_all(pmlxzj_state_t* ctx, FILE* f_src) {
  pmlxzj_state_e status = pmlxzj_init(ctx, f_src);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj exe failed: %d\n", status);
    return status;
  }
  status = pmlxzj_init_audio(ctx);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (audio) failed: %d\n", status);
    return status;
  }
  status = pmlxzj_init_frame(ctx);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (frame) failed: %d\n", status);
    return status;
  }

  return PMLXZJ_OK;
}
