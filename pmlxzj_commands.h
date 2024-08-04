#pragma once
#include <stdio.h>

int pmlxzj_cmd_extract_audio(int argc, char** argv);
int pmlxzj_cmd_print_info(int argc, char** argv);
int pmlxzj_cmd_unlock_exe(int argc, char** argv);

static inline void pmlxzj_usage(char* argv0) {
  printf("Usage: %s audio-dump <input> <output_audio>\n", argv0);
  printf("Usage: %s unlock <input> <output>\n", argv0);
  printf("Usage: %s info <input>\n", argv0);
}
