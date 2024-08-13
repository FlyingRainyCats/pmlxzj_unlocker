#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED (2)
#define PMLXZJ_AUDIO_TYPE_LOSSY_MP3 (5)
#define PMLXZJ_AUDIO_TYPE_TRUE_SPEECH (6)

static inline const char* pmlxzj_get_audio_codec_name(uint32_t audio_codec) {
  switch (audio_codec) {
    case PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED:
      return "PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED";
    case PMLXZJ_AUDIO_TYPE_LOSSY_MP3:
      return "PMLXZJ_AUDIO_TYPE_LOSSY_MP3";
    case PMLXZJ_AUDIO_TYPE_TRUE_SPEECH:
      return "PMLXZJ_AUDIO_TYPE_TRUE_SPEECH";
    default:
      return "UNKNOWN";
  }
}

#pragma pack(push, 1)
// size = 0xB4
typedef struct {
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
  uint32_t image_compress_type;
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

typedef struct {
  pmlxzj_initial_ui_state_t initial_ui_state;
  uint32_t edit_lock_nonce;
  uint32_t play_lock_password_checksum;
  uint32_t key_3;
  uint32_t key_4;
  uint32_t key_5;
  uint32_t unknown_3;
  uint32_t unknown_4;
  uint32_t offset_data_start;
  char magic[0x0C];
} pmlxzj_footer_t;

typedef struct {
  uint32_t field_0;
  uint32_t field_4;
  uint32_t field_8;
  int32_t total_frame_count;
  uint32_t field_10;
  uint32_t field_14;
  uint32_t field_18;
  uint32_t field_1C;
  uint32_t field_20;
  uint32_t field_24;  // some kind of special flag...
} pmlxzj_config_14d8_t;

typedef struct {
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;
} pmlxzj_wave_format_ex;
#pragma pack(pop)

typedef struct {
  FILE* file;

  long file_size;
  // encrypt_mode: 0(no_encryption), 1(EditLock), 2(PlayLock)
  int encrypt_mode;
  char nonce_buffer[20];
  pmlxzj_footer_t footer;
  uint32_t* idx1;
  size_t idx1_count;
  uint32_t* idx2;
  size_t idx2_count;

  pmlxzj_config_14d8_t field_14d8;
  long frame_metadata_offset;
  long first_frame_offset;
  long frame;

  // Audio frame
  uint32_t audio_segment_count;
  long audio_start_offset;

  // Audio: MP3
  long audio_mp3_start_offset;
  uint32_t* audio_mp3_chunk_offsets;
  uint32_t audio_mp3_total_size;
} pmlxzj_state_t;

typedef struct {
  char password[20];
  FILE* input_file;
  bool verbose;
  bool resume_on_bad_password;
} pmlxzj_user_params_t;

typedef enum {
  PMLXZJ_OK = 0,
  PMLXZJ_MAGIC_NOT_MATCH = 1,
  /** @deprecated */
  PMLXZJ_UNSUPPORTED_MODE_2 = 2,
  PMLXZJ_ALLOCATE_INDEX_LIST_ERROR = 3,
  PMLXZJ_SCAN_U32_ARRAY_ALLOC_ERROR = 4,
  PMLXZJ_AUDIO_OFFSET_UNSUPPORTED = 5,
  PMLXZJ_AUDIO_TYPE_UNSUPPORTED = 6,
  PMLXZJ_PASSWORD_ERROR = 7,
  PMLXZJ_AUDIO_INCORRECT_TYPE = 8,
  PMLXZJ_AUDIO_NOT_PRESENT = 9,
  PMLXZJ_GZIP_BUFFER_ALLOC_FAILURE = 10,
  PMLXZJ_GZIP_BUFFER_TOO_LARGE = 11,
  PMLXZJ_GZIP_INFLATE_FAILURE = 12,
  PMLXZJ_AUDIO_EMPTY_CHUNKS = 13,
} pmlxzj_state_e;

typedef enum {
  PMLXZJ_ENUM_CONTINUE = 0,
  PMLXZJ_ENUM_ABORT = 1,
} pmlxzj_enumerate_state_e;

typedef struct {
  // might be wrong
  uint32_t left;
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
} pmlxzj_rect;

typedef struct {
  int32_t frame_id;
  int32_t image_id;
  pmlxzj_rect cord;  // image position. all zero = full canvas size or missing
  uint32_t compressed_size;
  uint32_t decompressed_size;
} pmlxzj_frame_info_t;
typedef pmlxzj_enumerate_state_e(pmlxzj_enumerate_callback_t)(pmlxzj_state_t* ctx,
                                                              pmlxzj_frame_info_t* frame,
                                                              void* extra_callback_data);

uint32_t pmlxzj_password_checksum(const char* password);
pmlxzj_state_e pmlxzj_init(pmlxzj_state_t* ctx, pmlxzj_user_params_t* params);
pmlxzj_state_e pmlxzj_init_audio(pmlxzj_state_t* ctx);
pmlxzj_state_e pmlxzj_init_frame(pmlxzj_state_t* ctx);
pmlxzj_state_e pmlxzj_init_all(pmlxzj_state_t* ctx, pmlxzj_user_params_t* params);

// Frames / images
pmlxzj_enumerate_state_e pmlxzj_enumerate_images(pmlxzj_state_t* ctx,
                                                 pmlxzj_enumerate_callback_t* callback,
                                                 void* extra_callback_data);
// Audio
pmlxzj_state_e pmlxzj_audio_dump_to_file(pmlxzj_state_t* ctx, FILE* f_audio);
pmlxzj_state_e pmlxzj_audio_dump_mp3(pmlxzj_state_t* ctx, FILE* f_audio);
pmlxzj_state_e pmlxzj_audio_dump_compressed_wave(pmlxzj_state_t* ctx, FILE* f_audio);
