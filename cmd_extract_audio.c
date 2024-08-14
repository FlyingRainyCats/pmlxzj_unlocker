#include "pmlxzj.h"
#include "pmlxzj_commands.h"

#include <stdio.h>

int pmlxzj_cmd_extract_audio(int argc, char** argv) {
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
  status = pmlxzj_init_audio(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj audio failed: %d\n", status);
    return 1;
  }

  char* audio_output_path = argv[2];
  FILE* f_audio = fopen(audio_output_path, "wb");
  if (f_audio == NULL) {
    perror("ERROR: open audio out");
    return 1;
  }

  status = pmlxzj_audio_dump_to_file(&app, f_audio);
  if (status == PMLXZJ_OK) {
    fseek(f_audio, 0, SEEK_END);
    long audio_len = ftello(f_audio);
    printf("audio dump ok, len = %ld\n", audio_len);
  } else {
    // delete content as the output failed :c
    f_audio = freopen(audio_output_path, "wb" , f_audio);
  }
  fclose(f_audio);
  fclose(f_src);

  return 0;
}
