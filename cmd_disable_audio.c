#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_utils.h"

#include <memory.h>
#include <stdio.h>
#include <string.h>

int pmlxzj_cmd_disable_audio(int argc, char** argv) {
  if (argc <= 2) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  const char* exe_input_path = argv[1];
  const char* exe_output_path = argv[2];
  if (strcmp(exe_input_path, exe_output_path) == 0) {
    printf("ERROR: input and output file cannot be the same.\n");
    return 1;
  }

  FILE* f_src = fopen(exe_input_path, "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_user_params_t params = {0};
  params.input_file = f_src;
  pmlxzj_state_e status = pmlxzj_init(&app, &params);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj exe failed: %d\n", status);
    fclose(f_src);
    return 1;
  }

  if (app.audio_metadata_version == PMLXZJ_AUDIO_VERSION_LEGACY) {
    printf("ERROR: exe player upgrade required.\n");
    return 1;
  }

  FILE* f_dst = fopen(exe_output_path, "wb");
  if (f_dst == NULL) {
    perror("ERROR: open output");
    fclose(f_src);
    return 1;
  }

  pmlxzj_util_copy_file(f_dst, f_src);

  uint32_t audio_len = 0;
  fseek(f_dst, (long)app.footer.offset_data_start, SEEK_SET);
  fwrite(&audio_len, sizeof(audio_len), 1, f_dst);

  pmlxzj_footer_t footer = {0};
  memcpy(&footer, &app.footer, sizeof(footer));
  footer.config.audio_codec = PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED;
  fseek(f_dst, app.file_size - (long)(sizeof(pmlxzj_footer_t)), SEEK_SET);
  fwrite(&footer, sizeof(footer), 1, f_dst);

  fclose(f_dst);
  fclose(f_src);

  return 0;
}
