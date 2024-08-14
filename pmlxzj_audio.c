#include "pmlxzj.h"
#include "pmlxzj_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#define MAX_COMPRESSED_CHUNK_SIZE (0x1E848)
#define ZLIB_INFLATE_BUFFER_SIZE (1024)

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
      fseek(f_src, ctx->audio_metadata_offset + 18 + 4, SEEK_SET);
      pmlxzj_skip_lpe_data(f_src);
      pmlxzj_skip_lpe_data(f_src);
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
    printf("ERROR: failed to inflate audio (%d)\n", result);
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

  uint8_t* buffer = malloc(MAX_COMPRESSED_CHUNK_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate memory for audio buffer\n");
    return PMLXZJ_GZIP_BUFFER_ALLOC_FAILURE;
  }

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
  free(buffer);
  return result;
}

pmlxzj_state_e pmlxzj_audio_dump_mp3(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_MP3) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  if (ctx->audio_mp3_chunk_offsets == NULL) {
    return PMLXZJ_NO_AUDIO;
  }

  long start_offset = ctx->audio_stream_offset + (long)ctx->audio_mp3_chunk_offsets[0];
  long mp3_len = (long)ctx->audio_stream_size - (long)ctx->audio_mp3_chunk_offsets[0];

  fseek(ctx->file, start_offset, SEEK_SET);
  pmlxzj_util_copy(f_audio, ctx->file, mp3_len);
  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_audio_dump_aac(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.initial_ui_state.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_AAC) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  FILE* f_src = ctx->file;

  pmlxzj_util_copy(f_audio, ctx->file, ctx->audio_stream_size);

  return PMLXZJ_OK;
}
