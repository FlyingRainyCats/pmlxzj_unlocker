#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_enum_names.h"

#include <stdio.h>
#include <string.h>

int pmlxzj_cmd_extract_audio(int argc, char** argv) {
  if (argc <= 2) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  const char* exe_input_path = argv[1];
  const char* audio_output_path = argv[2];
  if (strcmp(exe_input_path, audio_output_path) == 0) {
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
    printf("ERROR: Init pmlxzj exe failed: %d (%s)\n", status, pmlxzj_get_state_name(status));
    fclose(f_src);
    return 1;
  }
  status = pmlxzj_init_audio(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj audio failed: %d (%s)\n", status, pmlxzj_get_state_name(status));
    fclose(f_src);
    return 1;
  }

  FILE* f_audio = fopen(audio_output_path, "wb");
  if (f_audio == NULL) {
    perror("ERROR: open audio out");
    fclose(f_src);
    return 1;
  }

  status = pmlxzj_audio_dump_to_file(&app, f_audio);
  if (status == PMLXZJ_OK) {
    fseek(f_audio, 0, SEEK_END);
    long audio_len = ftello(f_audio);
    printf("audio dump ok, len = %ld\n", audio_len);
  } else {
    printf("ERROR: failed to dump: %d (%s)\n", status, pmlxzj_get_state_name(status));
  }
  fclose(f_audio);
  fclose(f_src);

  return 0;
}
