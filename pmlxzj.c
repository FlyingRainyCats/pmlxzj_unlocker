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

pmlxzj_state_e pmlxzj_init(pmlxzj_state_t* ctx, pmlxzj_user_params_t* params) {
  FILE* f_src = params->input_file;

  memset(ctx, 0, sizeof(*ctx));
  ctx->file = f_src;

  fseek(f_src, 0, SEEK_END);
  ctx->file_size = ftell(f_src);

  fseek(f_src, -(int)sizeof(ctx->footer), SEEK_END);
  fread(&ctx->footer, sizeof(ctx->footer), 1, f_src);

  if (strcmp(ctx->footer.magic, "pmlxzjtlx") != 0) {
    return PMLXZJ_MAGIC_NOT_MATCH;
  }

  if (ctx->footer.edit_lock_nonce) {
    ctx->encrypt_mode = 1;
    char buffer[21] = {0};
    snprintf(buffer, sizeof(buffer) - 1, "%d", ctx->footer.edit_lock_nonce);
    for (int i = 1; i < 20; i++) {
      ctx->nonce_buffer[i] = buffer[20 - i];
    }
  } else if (ctx->footer.play_lock_password_checksum) {
    ctx->encrypt_mode = 2;

    uint32_t expected_checksum = ctx->footer.play_lock_password_checksum;
    uint32_t actual_checksum = pmlxzj_password_checksum(params->password);
    if (actual_checksum != expected_checksum) {
      fprintf(stderr, "ERROR: Password checksum mismatch.\n");
      fprintf(stderr, "       Expected: 0x%08x, got 0x%08x.\n", expected_checksum, actual_checksum);
      if (params->resume_on_bad_password) {
        fprintf(stderr, "WARN:  Continue without valid password...\n");
      } else {
        return PMLXZJ_PASSWORD_ERROR;
      }
    }

    char buffer[21] = {0};
    strncpy(buffer, params->password, sizeof(buffer));
    for (int i = 1; i < 20; i++) {
      ctx->nonce_buffer[i] = buffer[20 - i];
    }
  } else {
    ctx->encrypt_mode = 0;
  }

  fseek(f_src, (long)ctx->footer.offset_data_start, SEEK_SET);
  int32_t variant_i32 = 0;
  fread(&variant_i32, sizeof(variant_i32), 1, f_src);
  if (variant_i32 <= 0) {
    // New format
    ctx->audio_data_version = PMLXZJ_AUDIO_VERSION_CURRENT;
    ctx->audio_start_offset = (long)-variant_i32;
    ctx->frame_metadata_offset = (long)ctx->footer.offset_data_start + (long)sizeof(variant_i32);
  } else {
    // Legacy format
    ctx->audio_data_version = PMLXZJ_AUDIO_VERSION_LEGACY;
    ctx->frame_metadata_offset = (long)variant_i32;
    ctx->audio_start_offset = (long)ctx->footer.offset_data_start + (long)sizeof(variant_i32);
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

pmlxzj_state_e pmlxzj_init_all(pmlxzj_state_t* ctx, pmlxzj_user_params_t* params) {
  pmlxzj_state_e status = pmlxzj_init(ctx, params);
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

uint32_t pmlxzj_password_checksum(const char* password) {
  uint32_t checksum = 0x7D5;
  for (int i = 0; i < 20 && *password; i++) {
    uint8_t chr = *password++;
    checksum += chr * (i + i / 5 + 1);
  }
  return checksum;
}
