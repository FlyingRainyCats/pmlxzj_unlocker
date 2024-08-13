#pragma once
#include <stdio.h>
#include <libgen.h>

int pmlxzj_cmd_extract_audio(int argc, char** argv);
int pmlxzj_cmd_print_info(int argc, char** argv);
int pmlxzj_cmd_unlock_exe(int argc, char** argv);

static inline void pmlxzj_usage(char* argv0) {
  char* name = basename(argv0);
  printf("Usage:\n\n");
  printf("  %s audio-dump <input> <output>\n", name);
  printf("\n");
  printf("  %s unlock [-v] [-r] [-p password] [-P password.txt] <input> <output>\n", name);
  printf("       -r    Resume unlock, even when password checksum mismatch.\n");
  printf("       -p    Password.\n");
  printf("       -P    Password, but read from a text file instead.\n");
  printf("       -v    Verbose logging.\n");
  printf("\n");
  printf("  %s info [-v] [-i] [-f] <input>\n", name);
  printf("       -v    Verbose logging.\n");
  printf("       -i    Print indexes (unknown meaning).\n");
  printf("       -v    Print frames in the player data.\n");
}
