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

struct pmlxzj_commands_t {
  const char* name;
  int (*handler)(int argc, char** argv);
};

static struct pmlxzj_commands_t g_pmlxzj_commands[] = {
    {.name = "info", .handler = pmlxzj_cmd_print_info},
    {.name = "unlock", .handler = pmlxzj_cmd_unlock_exe},
    {.name = "audio-dump", .handler = pmlxzj_cmd_extract_audio},
    {.name = "audio-disable", .handler = pmlxzj_cmd_disable_audio},
};

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

  const size_t command_count = sizeof(g_pmlxzj_commands) / sizeof(g_pmlxzj_commands[0]);
  for (size_t i = 0; i < command_count; i++) {
    struct pmlxzj_commands_t* cmd = &g_pmlxzj_commands[i];
    if (strcmp(command, cmd->name) == 0) {
      return cmd->handler(argc, argv);
    }
  }

  pmlxzj_usage(argv[0]);
  return 1;
}
