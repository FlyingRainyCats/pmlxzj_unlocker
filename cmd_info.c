#include <unistd.h>

#include "pmlxzj.h"
#include "pmlxzj_commands.h"

struct pmlxzj_cmd_print_info_param_t {
  bool verbose;
};

pmlxzj_enumerate_state_e enum_frame_print_info(pmlxzj_state_t* app,
                                               pmlxzj_frame_info_t* frame,
                                               void* extra_callback_data) {
  struct pmlxzj_cmd_print_info_param_t* param = extra_callback_data;

  long frame_start_offset = ftell(app->file);
  if (frame->compressed_size > 10240) {
    long patch_start_offset = frame_start_offset + (long)frame->compressed_size / 2;
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x, patch=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset, (int)patch_start_offset);
  } else if (param->verbose) {
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset);
  }

  return PMLXZJ_ENUM_CONTINUE;
}

int pmlxzj_cmd_print_info(int argc, char** argv) {
  struct pmlxzj_cmd_print_info_param_t param = {0};
  int option = -1;
  while ((option = getopt(argc, argv, "v")) != -1) {
    if (option == 'v') {
      param.verbose = true;
    } else {
      fprintf(stderr, "ERROR: unknown option '-%c'\n", optopt);
      pmlxzj_usage(argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "ERROR: 'input' required.\n");
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[optind], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_state_e status = pmlxzj_init_all(&app, f_src);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj failed: %d\n", status);
    return 1;
  }

  printf("idx1(len=%d):\n", (int)app.idx1_count);
  for (size_t i = 0; i < app.idx1_count; i++) {
    printf("  idx1[%3d]: %11d / 0x%08x\n", (int)i, app.idx1[i], app.idx1[i]);
  }
  printf("idx2(len=%d):\n", (int)app.idx2_count);
  for (size_t i = 0; i < 20; i++) {
    printf("  idx2[%3d]: %11d / 0x%08x\n", (int)i, app.idx2[i], app.idx2[i]);
  }

  printf("offset_data_start: 0x%x\n", app.footer.offset_data_start);
  printf("frame offset: 0x%lx\n", app.first_frame_offset);
  printf("audio offset: 0x%lx\n", app.audio_start_offset);

  printf("encrypt mode 1 nonce: %d\n", app.footer.mode_1_nonce);
  printf("encrypt mode 2 nonce: %d\n", app.footer.mode_2_nonce);
  pmlxzj_enumerate_images(&app, enum_frame_print_info, &param);

  return 0;
}
