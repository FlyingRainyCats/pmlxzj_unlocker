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

  FILE* f_dst = fopen(exe_output_path, "wb");
  if (f_dst == NULL) {
    perror("ERROR: open output");
    fclose(f_src);
    return 1;
  }

  // Get the size of the input file
  fseek(f_src, 0, SEEK_END);
  size_t src_file_size = (size_t)ftell(f_src);
  fseek(f_src, 0, SEEK_SET);

  // Copy until the start of the data section
  pmlxzj_util_copy(f_dst, f_src, app.footer.offset_data_start);

  uint32_t zero = {0};
  fwrite(&zero, sizeof(zero), 1, f_dst);

  // Copy metadata
  if (app.audio_metadata_version == PMLXZJ_AUDIO_VERSION_LEGACY) {
    // Legacy: audio data followed by metadata and frame data.
    fseek(f_src, app.frame_metadata_offset, SEEK_SET);
    int64_t metadata_and_frame_size =
        (signed)src_file_size - app.frame_metadata_offset - (signed)sizeof(pmlxzj_footer_t);
    pmlxzj_util_copy(f_dst, f_src, metadata_and_frame_size);
  } else {
    // Current: metadata + frame data, followed by audio data, timecodes, and then header.
    // no easy way to strip audio data, let's just ignore them for now.
    fseek(f_src, sizeof(uint32_t), SEEK_CUR);
    int64_t data_size =
        (signed)src_file_size - app.footer.offset_data_start - (signed)sizeof(pmlxzj_footer_t) - 4;
    pmlxzj_util_copy(f_dst, f_src, data_size);
    fseek(f_dst, app.file_size - (long)(sizeof(pmlxzj_footer_t)), SEEK_SET);
  }

  pmlxzj_footer_t footer = {0};
  memcpy(&footer, &app.footer, sizeof(footer));
  footer.config.audio_codec = PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED;
  fwrite(&footer, sizeof(footer), 1, f_dst);

  fclose(f_dst);
  fclose(f_src);

  return 0;
}
