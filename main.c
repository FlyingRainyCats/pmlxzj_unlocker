#include <stdio.h>
#include <string.h>
#include "pmlxzj_commands.h"

int main(int argc, char** argv) {
  printf("pmlxzj_unlocker by AiFeiDeMao@52pojie.cn (FlyingRainyCats)\n");
  if (argc <= 2) {
    pmlxzj_usage(argv[0]);
    return 1;
  }

  if (_strcmpi(argv[1], "audio-dump") == 0) {
    return pmlxzj_cmd_extract_audio(argc, argv);
  } else if (_strcmpi(argv[1], "unlock") == 0) {
    return pmlxzj_cmd_unlock_exe(argc, argv);
  } else if (_strcmpi(argv[1], "info") == 0) {
    return pmlxzj_cmd_print_info(argc, argv);
  }

  pmlxzj_usage(argv[0]);
  return 1;
}
