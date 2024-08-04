#include "pmlxzj.h"
#include "utils.h"

#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>

void skip_sized_packet(FILE* f) {
  uint32_t skip = 0;
  fread(&skip, sizeof(skip), 1, f);
  if (skip != 0) {
    fseek(f, skip, SEEK_CUR);
  }
}

pmlxzj_state_e pmlxzj_init_frame(pmlxzj_state_t* ctx) {
  FILE* f = ctx->file;

  // init code from: TPlayForm_repareplay2
  fseek(f, ctx->footer.offset_data_start + 4 /* audio_offset */, SEEK_SET);

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
    hexdump(buff, seek_unused_data);
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
    hexdump(&test_frame_hdr, sizeof(test_frame_hdr));
    assert(memcmp(test_frame_hdr.data_hdr, "\x78\x9C", 2) == 0 && "invalid zlib data header");
  }
#endif

  return PMLXZJ_OK;
}

void pmlxzj_try_fix_frame(pmlxzj_state_t* ctx, FILE* f_dst) {
  uint32_t compressed_size = 0;
  fread(&compressed_size, sizeof(compressed_size), 1, ctx->file);

  long frame_pos = ftell(ctx->file);

  uint32_t decompressed_size = 0;
  fread(&decompressed_size, sizeof(decompressed_size), 1, ctx->file);

  if (compressed_size > 10240) {
    long key_pos = frame_pos + 4;
    long cipher_pos = frame_pos + (long)compressed_size / 2;

    char frame_key[20] = {0};
    char frame_ciphered[20] = {0};
    fseek(ctx->file, key_pos, SEEK_SET);
    fread(frame_key, sizeof(frame_key), 1, ctx->file);
    fseek(ctx->file, cipher_pos, SEEK_SET);
    fread(frame_ciphered, sizeof(frame_ciphered), 1, ctx->file);

    char* p_nonce_key = &ctx->nonce_buffer[20];
    for (int i = 0; i < 20; i++, p_nonce_key--) {
      frame_ciphered[i] = (char)(frame_ciphered[i] ^ frame_key[i] ^ *p_nonce_key);
    }
    fseek(f_dst, cipher_pos, SEEK_SET);
    fwrite(frame_ciphered, sizeof(frame_ciphered), 1, f_dst);

    printf("frame %d (len=(0x%x, 0x%x), key=0x%x, patch=0x%x)\n",  //
           (int)ctx->frame,                                        //
           compressed_size,                                        //
           (int)decompressed_size,                                 //
           (int)key_pos,                                           //
           (int)cipher_pos                                         //
    );
  }

  fseek(ctx->file, frame_pos + (long)compressed_size, SEEK_SET);
  ctx->frame++;
}
