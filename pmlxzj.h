#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// size = 0xB4
typedef struct __attribute__((__packed__)) {
  uint32_t color;
  uint32_t wnd_state;
  uint32_t field_8;
  uint32_t field_C;
  uint32_t field_10;
  uint32_t border_style_2;
  uint32_t field_18;
  uint32_t field_1C;
  uint32_t field_20;
  uint32_t field_24;
  uint32_t border_style_1;
  uint32_t field_2C;
  uint32_t field_30;
  uint32_t field_34;
  char wnd_title[24];
  uint32_t compress_type;
  uint32_t audio_codec;
  uint32_t field_58;
  uint32_t field_5C;
  uint32_t field_60;
  uint32_t field_64;
  uint32_t field_68;
  uint32_t field_6C;
  uint32_t field_70;
  uint32_t field_74;
  uint32_t field_78;
  uint32_t field_7C;
  uint32_t field_80;
  uint32_t field_84;
  uint32_t field_88;
  uint32_t field_8C;
  uint32_t field_90;
  uint32_t field_94;
  uint32_t field_98;
  uint32_t field_9C;
  uint32_t field_A0;
  uint32_t field_A4;
  uint32_t field_A8;
  uint32_t field_AC;
  uint32_t field_B0;
} pmlxzj_initial_ui_state_t;

typedef struct __attribute__((__packed__)) {
  pmlxzj_initial_ui_state_t initial_ui_state;
  uint32_t mode_1_nonce;
  uint32_t mode_2_nonce;
  uint32_t key_3;
  uint32_t key_4;
  uint32_t key_5;
  uint32_t unknown_3;
  uint32_t unknown_4;
  uint32_t offset_data_start;
  char magic[0x0C];
} pmlxzj_footer_t;

typedef struct __attribute__((__packed__)) {
  uint32_t field_0;
  uint32_t field_4;
  uint32_t field_8;
  uint32_t field_C;
  uint32_t field_10;
  uint32_t field_14;
  uint32_t field_18;
  uint32_t field_1C;
  uint32_t field_20;
  uint32_t field_24; // some kind of special flag...
} pmlxzj_config_14d8_t;

typedef struct {
  FILE* file;

  long file_size;
  // encrypt_mode: 0(no_encryption), 1(mode_1), 2(mode_2)
  int encrypt_mode;
  char nonce_buffer[21];
  pmlxzj_footer_t footer;
  uint32_t* idx1;
  size_t idx1_count;
  uint32_t* idx2;
  size_t idx2_count;

  pmlxzj_config_14d8_t field_14d8;
  long first_frame_offset;
  long frame;

  // Audio info
  uint32_t audio_segment_count;
  long audio_start_offset;
} pmlxzj_state_t;

typedef enum {
  PMLXZJ_OK = 0,
  PMLXZJ_MAGIC_NOT_MATCH = 1,
  PMLXZJ_UNSUPPORTED_MODE_2 = 2,
  PMLXZJ_ALLOCATE_INDEX_LIST_ERROR = 3,
  PMLXZJ_SCAN_U32_ARRAY_ALLOC_ERROR = 4,
  PMLXZJ_AUDIO_OFFSET_UNSUPPORTED = 5,
  PMLXZJ_AUDIO_TYPE_UNSUPPORTED = 6,
} pmlxzj_state_e;

pmlxzj_state_e pmlxzj_init(pmlxzj_state_t* ctx, FILE* f_src);
pmlxzj_state_e pmlxzj_init_audio(pmlxzj_state_t* ctx);
pmlxzj_state_e pmlxzj_init_frame(pmlxzj_state_t* ctx);
void pmlxzj_try_fix_frame(pmlxzj_state_t* ctx, FILE* f_dst);

bool pmlxzj_inflate_audio(pmlxzj_state_t* ctx, FILE* f_audio);
