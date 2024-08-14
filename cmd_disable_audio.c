#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_utils.h"

#include <stdio.h>
#include <memory.h>

int pmlxzj_cmd_disable_audio(int argc, char** argv) {
  if (argc <= 2) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[1], "rb");
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
    return 1;
  }

  if (app.audio_data_version == PMLXZJ_AUDIO_VERSION_LEGACY) {
    printf("ERROR: exe player upgrade required.\n");
    return 1;
  }

  char* audio_output_path = argv[2];
  FILE* f_dst = fopen(audio_output_path, "wb");
  if (f_dst == NULL) {
    perror("ERROR: open output");
    return 1;
  }

  pmlxzj_util_copy_file(f_dst, f_src);

  uint32_t audio_len = 0;
  fseek(f_dst, (long)app.footer.offset_data_start, SEEK_SET);
  fwrite(&audio_len, sizeof(audio_len), 1, f_dst);

  pmlxzj_footer_t footer = {0};
  memcpy(&footer, &app.footer, sizeof(footer));
  footer.initial_ui_state.audio_codec = PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED;
  fseek(f_dst, app.file_size - (long)(sizeof(pmlxzj_footer_t)), SEEK_SET);
  fwrite(&footer, sizeof(footer), 1, f_dst);

  fclose(f_dst);
  fclose(f_src);

  return 0;
}
