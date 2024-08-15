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
  switch (ctx->footer.config.audio_codec) {
    case PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED: {
      pmlxzj_audio_wav_zlib_t* audio = &ctx->audio.wav_zlib;

      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);
      fread(&audio->segment_count, sizeof(audio->segment_count), 1, f_src);
      audio->offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_WAVE_RAW: {
      pmlxzj_audio_wav_t* audio = &ctx->audio.wav;

      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);
      fread(&audio->size, sizeof(audio->size), 1, f_src);
      audio->offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_MP3: {
      pmlxzj_audio_mp3_t* audio = &ctx->audio.mp3;

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

      audio->segment_count = mp3_chunk_count + 1;
      audio->offsets = calloc(audio->segment_count, sizeof(uint32_t));
      fread(audio->offsets, sizeof(uint32_t), audio->segment_count, f_src);
      fread(&audio->size, sizeof(audio->size), 1, f_src);
      assert(audio->offsets[0] == 0);
      audio->offsets[mp3_chunk_count] = audio->size;
      audio->offset = ftell(f_src);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_AAC: {
      pmlxzj_audio_aac_t* audio = &ctx->audio.aac;

      fseek(f_src, ctx->audio_metadata_offset, SEEK_SET);

      pmlxzj_wave_format_ex wav_spec;
      uint32_t unused_field_80;

      fread(&wav_spec, sizeof(wav_spec), 1, f_src);
      fread(&unused_field_80, sizeof(unused_field_80), 1, f_src);

      uint32_t format_len;
      fread(&format_len, sizeof(format_len), 1, f_src);
      if (format_len > sizeof(audio->format)) {
        return PMLXZJ_AUDIO_AAC_INVALID_DECODER_SPECIFIC_INFO;
      }
      fread(&audio->format, format_len, 1, f_src);
      pmlxzj_state_e state = pmlxzj_aac_parse_decoder_specific_info(&audio->config, audio->format, format_len);
      if (state != PMLXZJ_OK) {
        return state;
      }

      uint32_t segment_count_in_bytes;
      fread(&segment_count_in_bytes, sizeof(segment_count_in_bytes), 1, f_src);
      audio->segment_sizes = malloc(segment_count_in_bytes);
      audio->segment_count = segment_count_in_bytes / sizeof(uint32_t);
      fread(audio->segment_sizes, 1, segment_count_in_bytes, f_src);

      for (uint32_t i = 0; i < audio->segment_count; i++) {
        if (audio->segment_sizes[i] > PMLXZJ_AAC_MAX_FRAME_DATA_LEN) {
          free(audio->segment_sizes);
          audio->segment_sizes = NULL;
          return PMLXZJ_AUDIO_AAC_INVALID_FRAME_SIZE;
        }
      }

      fread(&audio->size, sizeof(audio->size), 1, f_src);
      audio->offset = ftell(f_src);
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
  switch (ctx->footer.config.audio_codec) {
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
  if (ctx->footer.config.audio_codec != PMLXZJ_AUDIO_TYPE_WAVE_RAW) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  pmlxzj_audio_wav_t* audio = &ctx->audio.wav;

  FILE* f_src = ctx->file;
  fseek(f_src, audio->offset, SEEK_SET);
  pmlxzj_util_copy(f_audio, ctx->file, audio->size);
  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_audio_dump_compressed_wave(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.config.audio_codec != PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  pmlxzj_audio_wav_zlib_t* audio = &ctx->audio.wav_zlib;

  FILE* f_src = ctx->file;
  uint32_t len = audio->segment_count;
  fseek(f_src, audio->offset, SEEK_SET);

  pmlxzj_state_e result = PMLXZJ_OK;
  uint8_t buffer[MAX_COMPRESSED_CHUNK_SIZE] = {0};
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
  if (ctx->footer.config.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_MP3) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  pmlxzj_audio_mp3_t* audio = &ctx->audio.mp3;

  fseek(ctx->file, audio->offset, SEEK_SET);
  pmlxzj_util_copy(f_audio, ctx->file, audio->size);
  return PMLXZJ_OK;
}

pmlxzj_state_e pmlxzj_audio_dump_aac(pmlxzj_state_t* ctx, FILE* f_audio) {
  if (ctx->footer.config.audio_codec != PMLXZJ_AUDIO_TYPE_LOSSY_AAC) {
    return PMLXZJ_AUDIO_INCORRECT_TYPE;
  }

  pmlxzj_audio_aac_t* audio = &ctx->audio.aac;
  fseek(ctx->file, audio->offset, SEEK_SET);
  uint8_t buffer[PMLXZJ_AAC_ADTS_HEADER_LEN + PMLXZJ_AAC_MAX_FRAME_DATA_LEN] = {0};

  uint32_t n = audio->segment_count;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t chunk_size = audio->segment_sizes[i];
    assert(chunk_size < PMLXZJ_AAC_MAX_FRAME_DATA_LEN);

    size_t header_len = pmlxzj_aac_adts_header(buffer, &audio->config, chunk_size);
    assert(header_len == PMLXZJ_AAC_ADTS_HEADER_LEN);

    size_t bytes_read = fread(&buffer[header_len], 1, chunk_size, ctx->file);
    assert(bytes_read == chunk_size);

    fwrite(buffer, 1, header_len + bytes_read, f_audio);
  }
  assert(ftell(ctx->file) == (long)(ctx->audio_stream_offset + ctx->audio_stream_size));

  return PMLXZJ_OK;
}
