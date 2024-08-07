#include "pmlxzj.h"
#include "pmlxzj_commands.h"

#include <stdio.h>

int pmlxzj_cmd_extract_audio(int argc, char** argv) {
  if (argc <= 3) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[2], "rb");
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

  printf("data_start_offset = 0x%08x\n", app.footer.offset_data_start);
  printf("data_audio_offset = 0x%08x (segment.len=%d)\n", (int)app.audio_start_offset, (int)app.audio_segment_count);

  FILE* f_audio = fopen(argv[3], "wb");
  if (f_audio == NULL) {
    perror("ERROR: open audio out");
    return 1;
  }
  pmlxzj_inflate_audio(&app, f_audio);
  return 0;
}
