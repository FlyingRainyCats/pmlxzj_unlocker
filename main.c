#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pmlxzj_commands.h"
#include "pmlxzj_win32.h"

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "0.0.0-unknown"
#endif

#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

int main(int argc, char** argv) {
  FixWindowsUnicodeSupport(&argv);

  printf("pmlxzj_unlocker v" PROJECT_VERSION " by 爱飞的猫@52pojie.cn (FlyingRainyCats)\n");
  if (argc <= 2) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  char* command = argv[1];
  argv[1] = argv[0];
  argv++;
  argc--;

  if (strcmp(command, "audio-dump") == 0) {
    return pmlxzj_cmd_extract_audio(argc, argv);
  } else if (strcmp(command, "unlock") == 0) {
    return pmlxzj_cmd_unlock_exe(argc, argv);
  } else if (strcmp(command, "info") == 0) {
    return pmlxzj_cmd_print_info(argc, argv);
  }

  pmlxzj_usage(argv[0]);
  return 1;
}
