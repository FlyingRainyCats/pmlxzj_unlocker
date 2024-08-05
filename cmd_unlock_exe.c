#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_utils.h"

#include <stddef.h>

pmlxzj_enumerate_state_e enum_frame_patch_mode_1(pmlxzj_state_t* app, pmlxzj_frame_info_t* frame, void* extra) {
  FILE* f_dst = extra;
  long frame_start_offset = ftell(app->file);

  if (frame->compressed_size > 10240) {
    long key_pos = frame_start_offset + 4;
    long cipher_pos = frame_start_offset + (long)frame->compressed_size / 2;

    char frame_key[20];
    char frame_ciphered[20];
    fseek(app->file, key_pos, SEEK_SET);
    fread(frame_key, sizeof(frame_key), 1, app->file);
    fseek(app->file, cipher_pos, SEEK_SET);
    fread(frame_ciphered, sizeof(frame_ciphered), 1, app->file);

    for (int i = 0; i < 20; i++) {
      frame_ciphered[i] = (char)(frame_ciphered[i] ^ frame_key[i] ^ app->nonce_buffer_mode_1[i]);
    }
    fseek(f_dst, cipher_pos, SEEK_SET);
    fwrite(frame_ciphered, sizeof(frame_ciphered), 1, f_dst);

    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x, patch=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset, (int)cipher_pos);
  } else {
#ifndef NDEBUG
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset);
#endif
  }

  return PMLXZJ_ENUM_CONTINUE;
}

int pmlxzj_cmd_unlock_exe(int argc, char** argv) {
  if (argc <= 3) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[2], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  FILE* f_dst = fopen(argv[3], "wb");
  if (f_dst == NULL) {
    perror("ERROR: open dest input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_state_e status = pmlxzj_init_all(&app, f_src);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init failed: %d\n", status);
    return 1;
  }

  pmlxzj_util_copy_file(f_dst, f_src);

  if (app.footer.mode_1_nonce != 0) {
    pmlxzj_enumerate_images(&app, enum_frame_patch_mode_1, f_dst);

    // remove nonce_key_mode_1
    uint32_t nonce_key_mode_1 = 0;
    fseek(f_dst, app.file_size - (long)(sizeof(pmlxzj_footer_t) - offsetof(pmlxzj_footer_t, mode_1_nonce)), SEEK_SET);
    fwrite(&nonce_key_mode_1, sizeof(nonce_key_mode_1), 1, f_dst);
  } else {
    printf("error: mode_1_nonce is zero. Unsupported cipher or not encrypted.\n");
  }

  return 0;
}
