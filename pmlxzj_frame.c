#include "pmlxzj.h"
#include "pmlxzj_utils.h"

#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>

void skip_sized_packet(FILE* f) {
  uint32_t skip = 0;
  fread(&skip, sizeof(skip), 1, f);
  if (skip != 0) {
    fseek(f, (long)skip, SEEK_CUR);
  }
}

pmlxzj_state_e pmlxzj_init_frame(pmlxzj_state_t* ctx) {
  FILE* f = ctx->file;

  // init code from: TPlayForm_repareplay2
  fseek(f, (long)ctx->footer.offset_data_start + 4 /* audio_offset */, SEEK_SET);

  fread(&ctx->field_14d8, sizeof(ctx->field_14d8), 1, f);

  bool f24 = ctx->field_14d8.field_24 == 1;
  if (!f24) {
    fprintf(stderr, "WARN: ctx->field_14d8.field_24 == %d. Frame init may fail.\n", ctx->field_14d8.field_24);
  }

  const long seek_unused_data = 20 /* v182 */ + 20 /* str1 */ + 40 /* v181 */ +
                                4 * 3 /* field_1484/field_1488/field_1490 */ + 20 /* font: "微软雅黑" */
                                + 4 /* field_1498 */ + 4 /* off_149C_bitmask */;

#ifndef NDEBUG
  {
    printf("DEBUG (unused data):\n");
    char buff[seek_unused_data];
    fread(buff, seek_unused_data, 1, f);
    pmlxzj_util_hexdump(buff, seek_unused_data);
  }
#else
  fseek(f, seek_unused_data, SEEK_CUR);
#endif

  if (f24) {
    fseek(f, 8, SEEK_CUR);
    skip_sized_packet(f);  // stream2 setup
  }

  ctx->first_frame_offset = ftell(f);
#ifndef NDEBUG
  {
    printf("DEBUG (test first frame):\n");
    struct __attribute__((__packed__)) {
      uint32_t compressed;
      uint32_t decompressed;
      char data_hdr[2];
    } test_frame_hdr;
    fread(&test_frame_hdr, sizeof(test_frame_hdr), 1, f);
    pmlxzj_util_hexdump(&test_frame_hdr, sizeof(test_frame_hdr));
    assert(memcmp(test_frame_hdr.data_hdr, "\x78\x9C", 2) == 0 && "invalid zlib data header");
  }
#endif

  return PMLXZJ_OK;
}

pmlxzj_enumerate_state_e pmlxzj_enumerate_images(pmlxzj_state_t* ctx,
                                                 pmlxzj_enumerate_callback_t* callback,
                                                 void* extra_callback_data) {
  pmlxzj_enumerate_state_e enum_state = PMLXZJ_ENUM_CONTINUE;
  FILE* file = ctx->file;
  bool field_24_set = ctx->field_14d8.field_24 == 1;
  fseek(file, ctx->first_frame_offset, SEEK_SET);
  pmlxzj_frame_info_t frame_info = {0};

  // process first frame
  {
    fread(&frame_info.compressed_size, sizeof(frame_info.compressed_size), 1, file);
    long pos = ftell(file);
    fread(&frame_info.decompressed_size, sizeof(frame_info.decompressed_size), 1, file);
    fseek(file, -4, SEEK_CUR);
    enum_state = callback(ctx, &frame_info, extra_callback_data);
    frame_info.image_id++;

    fseek(file, pos + (long)frame_info.compressed_size, SEEK_SET);
    fread(&frame_info.frame_id, sizeof(frame_info.frame_id), 1, file);
    assert(frame_info.frame_id == 0);
    if (field_24_set) {
      fseek(file, 8, SEEK_CUR);  // f24: timestamps?
    }
  }

  int32_t last_frame_id = -(ctx->field_14d8.total_frame_count - 1);
  while (enum_state == PMLXZJ_ENUM_CONTINUE && last_frame_id != frame_info.frame_id) {
    // seek stream2
    if (field_24_set) {
      uint32_t stream2_len = 0;
      fread(&stream2_len, sizeof(stream2_len), 1, file);
      if (stream2_len != 0) {
        fseek(file, (long)stream2_len, SEEK_CUR);
      }
    }

    // Read frame id. If `frame_id` is negative, the frame uses previous content from previous frame.
    fread(&frame_info.frame_id, sizeof(frame_info.frame_id), 1, file);
    while (enum_state == PMLXZJ_ENUM_CONTINUE && frame_info.frame_id > 0) {
      int32_t eof_mark;

      // read cords
      fread(&frame_info.cord, sizeof(frame_info.cord), 1, file);

      // read compressed size and decompressed size, and a special eof mark (not used?)
      fread(&frame_info.compressed_size, sizeof(frame_info.compressed_size), 1, file);
      long pos = ftell(file);
      fread(&frame_info.decompressed_size, sizeof(frame_info.decompressed_size), 1, file);
      fread(&eof_mark, sizeof(eof_mark), 1, file);

      if ((int32_t)frame_info.decompressed_size == -1 && eof_mark == -1) {
        fprintf(stderr, "WARN: unknown handling of frame, skip");
      } else {
        fseek(file, -8, SEEK_CUR);
        enum_state = callback(ctx, &frame_info, extra_callback_data);
      }
      frame_info.image_id++;
      fseek(file, pos + (long)frame_info.compressed_size, SEEK_SET);
      fread(&frame_info.frame_id, sizeof(frame_info.frame_id), 1, file);  // read next frame id.
    }

    if (field_24_set) {
      fseek(file, 8, SEEK_CUR);
    }
  }

  return enum_state;
}
