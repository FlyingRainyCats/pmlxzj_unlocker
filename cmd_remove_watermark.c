#include <unistd.h>

#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_utils.h"

typedef struct {
  bool verbose;
  bool remove_unregistered_watermark;
  bool remove_playback_text_watermark;
} pmlxzj_cmd_remove_watermark_param_t;

int pmlxzj_cmd_remove_watermark(int argc, char** argv) {
  pmlxzj_cmd_remove_watermark_param_t param = {0};
  int option = -1;
  while ((option = getopt(argc, argv, "vrt")) != -1) {
    switch (option) {
      case 'v':
        param.verbose = true;
        break;
      case 'r':
        param.remove_unregistered_watermark = true;
        break;
      case 't':
        param.remove_playback_text_watermark = true;
        break;
      default:
        fprintf(stderr, "ERROR: unknown option '-%c'\n", optopt);
        pmlxzj_usage(argv[0]);
        return 1;
    }
  }

  // Default to remove both watermarks, if none specified
  if (!param.remove_unregistered_watermark && !param.remove_playback_text_watermark) {
    param.remove_unregistered_watermark = true;
    param.remove_playback_text_watermark = true;
  }

  if (argc < optind + 2) {
    fprintf(stderr, "ERROR: missing parameters for input and output");
    pmlxzj_usage(argv[0]);
    return 1;
  }

  const char* exe_input_path = argv[optind];
  const char* exe_output_path = argv[optind + 1];
  if (strcmp(exe_input_path, exe_output_path) == 0) {
    printf("ERROR: input and output file cannot be the same.\n");
    return 1;
  }

  FILE* f_src = fopen(exe_input_path, "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  FILE* f_dst = fopen(exe_output_path, "wb");
  if (f_dst == NULL) {
    perror("ERROR: open dest input");
    fclose(f_src);
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_user_params_t params = {0};
  params.input_file = f_src;
  pmlxzj_state_e status = pmlxzj_init_all(&app, &params);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj exe failed: %d\n", status);
    fclose(f_src);
    return 1;
  }

  pmlxzj_watermark_t watermark = app.watermark;
  if (param.remove_unregistered_watermark) {
    static uint8_t lic_data_1[20] = {0x41, 0x69, 0x46, 0x65, 0x69, 0x44, 0x65, 0x4D, 0x61, 0x6F,
                                     0x40, 0x35, 0x32, 0x70, 0x6F, 0x6A, 0x69, 0x65, 0x00, 0x00};
    static uint8_t lic_data_2[20] = {0x4C, 0x53, 0x48, 0x4E, 0x48, 0x53, 0x47, 0x4D, 0x48, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(watermark.lic_data_1, lic_data_1, sizeof(lic_data_1));
    memcpy(watermark.lic_data_2, lic_data_2, sizeof(lic_data_2));
  }

  if (param.remove_playback_text_watermark) {
    static uint8_t blank_playback_watermark[20] = {0x64, 0x21, 0x41, 0x69, 0x46, 0x65, 0x69, 0x44, 0x65, 0x4D,
                                                   0x61, 0x6F, 0x40, 0x35, 0x32, 0x70, 0x6F, 0x6A, 0x69, 0x65};
    memcpy(watermark.user_watermark, blank_playback_watermark, sizeof(blank_playback_watermark));
  }

  pmlxzj_util_copy_file(f_dst, f_src);
  fseek(f_dst, app.watermark_offset, SEEK_SET);
  fwrite(&watermark, sizeof(watermark), 1, f_dst);

  return 0;
}
