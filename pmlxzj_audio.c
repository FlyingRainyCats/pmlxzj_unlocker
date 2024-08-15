#include "pmlxzj.h"
#include "pmlxzj_audio_aac.h"
#include "pmlxzj_enum_names.h"
#include "pmlxzj_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#define MAX_COMPRESSED_CHUNK_SIZE (0x1E848)
#define ZLIB_INFLATE_BUFFER_SIZE (4096)

pmlxzj_state_e pmlxzj_init_audio(pmlxzj_state_t* ctx) {
  if (ctx->audio_metadata_offset == 0) {
    // no audio
    return PMLXZJ_NO_AUDIO;
  }

  FILE* f_src = ctx->file;
  switch (ctx->footer.initial_ui_state.audio_codec) {
    case PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED: {
      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);
      fread(&ctx->audio_segment_count, sizeof(ctx->audio_segment_count), 1, f_src);
      ctx->audio_stream_offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_WAVE_RAW: {
      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);
      fread(&ctx->audio_stream_size, sizeof(ctx->audio_stream_size), 1, f_src);
      ctx->audio_stream_offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_MP3: {
      assert(sizeof(pmlxzj_wave_format_ex) == 0x12);
      pmlxzj_wave_format_ex wav_spec;
      pmlxzj_wave_format_ex mp3_spec;
      uint32_t unk_audio_prop;
      uint32_t mp3_chunk_count;

      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);
      fread(&wav_spec, sizeof(wav_spec), 1, f_src);
      fread(&mp3_spec, sizeof(mp3_spec), 1, f_src);
      fread(&unk_audio_prop, sizeof(unk_audio_prop), 1, f_src);
      fread(&mp3_chunk_count, sizeof(mp3_chunk_count), 1, f_src);
      if (mp3_chunk_count == 0) {
        break;
      }

      ctx->audio_segment_count = mp3_chunk_count + 1;
      ctx->audio_mp3_chunk_offsets = calloc(ctx->audio_segment_count, sizeof(uint32_t));
      fread(ctx->audio_mp3_chunk_offsets, sizeof(uint32_t), ctx->audio_segment_count, f_src);
      fread(&ctx->audio_stream_size, sizeof(ctx->audio_stream_size), 1, f_src);
      assert(ctx->audio_mp3_chunk_offsets[0] == 0);
      ctx->audio_mp3_chunk_offsets[mp3_chunk_count] = ctx->audio_stream_size;
      ctx->audio_stream_offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_AAC: {
      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);

      pmlxzj_wave_format_ex wav_spec;
      uint32_t unused_field_80;

      fread(&wav_spec, sizeof(wav_spec), 1, f_src);
      fread(&unused_field_80, sizeof(unused_field_80), 1, f_src);

      uint32_t decoder_info_len;
      fread(&decoder_info_len, sizeof(decoder_info_len), 1, f_src);
      if (decoder_info_len > sizeof(ctx->audio_aac_decoder_data)) {
        return PMLXZJ_AUDIO_AAC_INVALID_DECODER_SPECIFIC_INFO;
      }
      fread(&ctx->audio_aac_decoder_data, decoder_info_len, 1, f_src);
      pmlxzj_state_e state =
          pmlxzj_aac_parse_decoder_specific_info(&ctx->audio_aac_config, ctx->audio_aac_decoder_data, decoder_info_len);
      if (state != PMLXZJ_OK) {
        return state;
      }

      uint32_t segment_count_in_bytes;
      fread(&segment_count_in_bytes, sizeof(segment_count_in_bytes), 1, f_src);
      ctx->audio_aac_chunk_sizes = malloc(segment_count_in_bytes);
      ctx->audio_segment_count = segment_count_in_bytes / sizeof(uint32_t);
      fread(ctx->audio_aac_chunk_sizes, 1, segment_count_in_bytes, f_src);

      for (uint32_t i = 0; i < ctx->audio_segment_count; i++) {
        if (ctx->audio_aac_chunk_sizes[i] > PMLXZJ_AAC_MAX_FRAME_DATA_LEN) {
          free(ctx->audio_aac_chunk_sizes);
          ctx->audio_aac_chunk_sizes = NULL;
          return PMLXZJ_AUDIO_AAC_INVALID_FRAME_SIZE;
        }
      }

      fread(&ctx->audio_stream_size, sizeof(ctx->audio_stream_size), 1, f_src);
      ctx->audio_stream_offset = ftell(f_src);
    }
  }

  return PMLXZJ_OK;
}

bool inflate_chunk(FILE* output, uint8_t* input, size_t length) {
  z_stream strm = {0};
  strm.next_in = (Bytef*)input;
  strm.avail_in = (uInt)length;

  if (inflateInit(&strm) != Z_OK) {
    fprintf(stderr, "Failed to initialize inflate\n");
    return false;
  }

  bool ok = true;
  char buffer[ZLIB_INFLATE_BUFFER_SIZE];

  do {
    strm.next_out = (Bytef*)buffer;
    strm.avail_out = ZLIB_INFLATE_BUFFER_SIZE;

    int ret = inflate(&strm, Z_NO_FLUSH);

    if (ret == Z_STREAM_ERROR) {
      fprintf(stderr, "Zlib stream error\n");
      ok = false;
      break;
    }

    if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
      fprintf(stderr, "Zlib inflate error %s\n", strm.msg);
      ok = false;
      break;
    }

    size_t output_length = ZLIB_INFLATE_BUFFER_SIZE - strm.avail_out;
    fwrite(buffer, 1, output_length, output);
  } while (strm.avail_out == 0);

  inflateEnd(&strm);
  return ok;
}

pmlxzj_state_e pmlxzj_audio_dump_to_file(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->audio_metadata_offset == 0) {
    fprintf(stderr, "File does not contain audio or not initialized.\n");
    return PMLXZJ_AUDIO_NOT_PRESENT;
  }

  pmlxzj_state_e result = PMLXZJ_AUDIO_TYPE_UNSUPPORTED;
  switch (ctx->footer.initial_ui_state.audio_codec) {
    case PMLXZJ_AUDIO_TYPE_LOSSY_MP3:
      result = pmlxzj_audio_dump_mp3(ctx, f_audio);
      break;

    case PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED:
      result = pmlxzj_audio_dump_compressed_wave(ctx, f_audio);
      break;

    case PMLXZJ_AUDIO_TYPE_WAVE_RAW:
      result = pmlxzj_audio_dump_raw_wave(ctx, f_audio);
      break;

    case PMLXZJ_AUDIO_TYPE_LOSSY_AAC:
      result = pmlxzj_audio_dump_aac(ctx, f_audio);
      break;

    default:
      break;
  }

  if (result != PMLXZJ_OK) {
    printf("ERROR: %s: failed to inflate audio: %d (%s)", __func__, result, pmlxzj_get_state_name(result));
  }
  return result;
}

pmlxzj_state_e pmlxzj_audio_dump_raw_wave(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_WAVE_RAW) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  FILE* f_src = ctx->file;
  fseek(f_src, ctx->audio_stream_offset, SEEK_SET);
  pmlxzj_util_copy(f_audio, ctx->file, ctx->audio_stream_size);
  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_audio_dump_compressed_wave(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  uint8_t buffer[MAX_COMPRESSED_CHUNK_SIZE] = {0};
  FILE* f_src = ctx->file;
  fseek(f_src, ctx->audio_stream_offset, SEEK_SET);
  uint32_t len = ctx->audio_segment_count;
  pmlxzj_state_e result = PMLXZJ_OK;
  for (uint32_t i = 0; i < len; i++) {
    uint32_t chunk_size = 0;
    fread(&chunk_size, sizeof(chunk_size), 1, f_src);
    if (chunk_size > MAX_COMPRESSED_CHUNK_SIZE) {
      fprintf(stderr, "compressed audio chunk size too large (chunk=%d, offset=%ld)\n", i, ftell(f_src));
      result = PMLXZJ_GZIP_BUFFER_TOO_LARGE;
      break;
    }
    fread(buffer, chunk_size, 1, f_src);
    if (!inflate_chunk(f_audio, buffer, chunk_size)) {
      result = PMLXZJ_GZIP_INFLATE_FAILURE;
      break;
    }
  }
  return result;
}

pmlxzj_state_e pmlxzj_audio_dump_mp3(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_MP3) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  if (ctx->audio_mp3_chunk_offsets == NULL) {
    return PMLXZJ_NO_AUDIO;
  }

  fseek(ctx->file, ctx->audio_stream_offset, SEEK_SET);
  pmlxzj_util_copy(f_audio, ctx->file, ctx->audio_stream_size);
  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_audio_dump_aac(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_AAC) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  fseek(ctx->file, ctx->audio_stream_offset, SEEK_SET);
  uint8_t buffer[PMLXZJ_AAC_ADTS_HEADER_LEN + PMLXZJ_AAC_MAX_FRAME_DATA_LEN] = {0};
  for (uint32_t i = 0; i < ctx->audio_segment_count; i++) {
    uint32_t chunk_size = ctx->audio_aac_chunk_sizes[i];
    assert(chunk_size < PMLXZJ_AAC_MAX_FRAME_DATA_LEN);

    size_t header_len = pmlxzj_aac_adts_header(buffer, &ctx->audio_aac_config, chunk_size);
    assert(header_len == PMLXZJ_AAC_ADTS_HEADER_LEN);

    size_t bytes_read = fread(&buffer[header_len], 1, chunk_size, ctx->file);
    assert(bytes_read == chunk_size);

    fwrite(buffer, 1, header_len + bytes_read, f_audio);
  }
  assert(ftell(ctx->file) == (long)(ctx->audio_stream_offset + ctx->audio_stream_size));

  return PMLXZJ_OK;
}
