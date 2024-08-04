#include "pmlxzj.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#define MAX_COMPRESSED_CHUNK_SIZE (0x1E848)

bool inflate_chunk(FILE* output, uint8_t* input, size_t length) {
  z_stream strm = {0};
  strm.next_in = (Bytef*)input;
  strm.avail_in = length;

  if (inflateInit(&strm) != Z_OK) {
    fprintf(stderr, "Failed to initialize inflate\n");
    return false;
  }

  bool ok = true;
  const size_t BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE];

  do {
    strm.next_out = (Bytef*)buffer;
    strm.avail_out = BUFFER_SIZE;

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

    size_t output_length = BUFFER_SIZE - strm.avail_out;
    fwrite(buffer, 1, output_length, output);
  } while (strm.avail_out == 0);

  inflateEnd(&strm);
  return ok;
}

bool pmlxzj_inflate_audio(pmlxzj_state_t* ctx, FILE* f_audio) {
  uint8_t* buffer = malloc(MAX_COMPRESSED_CHUNK_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate memory for audio buffer\n");
    return false;
  }

  FILE* f_src = ctx->file;
  fseek(f_src, ctx->audio_start_offset, SEEK_SET);
  uint32_t len = ctx->audio_segment_count;
  bool ok = true;
  for (uint32_t i = 0; ok && i < len; i++) {
    uint32_t chunk_size = 0;
    fread(&chunk_size, sizeof(chunk_size), 1, f_src);
    if (chunk_size > MAX_COMPRESSED_CHUNK_SIZE) {
      fprintf(stderr, "compressed audio chunk size too large (chunk=%d, offset=%ld)\n", i, ftell(f_src));
      ok = false;
      break;
    }
    fread(buffer, chunk_size, 1, f_src);
    ok = inflate_chunk(f_audio, buffer, chunk_size);
  }
  free(buffer);
  return ok;
}

pmlxzj_state_e pmlxzj_init_audio(pmlxzj_state_t* ctx) {
  // 2: wav
  if (ctx->footer.initial_ui_state.audio_codec != 2) {
    return PMLXZJ_AUDIO_TYPE_UNSUPPORTED;
  }

  FILE* f_src = ctx->file;
  fseek(f_src, (long)ctx->footer.offset_data_start, SEEK_SET);
  int32_t audio_offset = 0;
  fread(&audio_offset, sizeof(audio_offset), 1, f_src);
  if (audio_offset == 0) {
    // no audio
    ctx->audio_segment_count = 0;
    ctx->audio_start_offset = 0;
    return PMLXZJ_OK;
  }
  if (audio_offset > 0) {
    return PMLXZJ_AUDIO_OFFSET_UNSUPPORTED;
  }
  fseek(f_src, -audio_offset, SEEK_SET);
  fread(&ctx->audio_segment_count, sizeof(ctx->audio_segment_count), 1, f_src);
  ctx->audio_start_offset = ftell(f_src);
  return PMLXZJ_OK;
}
