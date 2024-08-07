#pragma once

#include "pmlxzj_utils.h"

#ifdef _WIN32
#include <locale.h>
#include <windows.h>

#include <shellapi.h>

static inline char* Win32W2U8(HANDLE hHeap, const wchar_t* text) {
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
  char* result = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, size_needed * sizeof(char));
  if (result != NULL) {
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result, size_needed, NULL, NULL);
  }
  return result;
}

static inline void FixWindowsUnicodeSupport(char*** argv) {
  PMLXZJ_UNUSED_PARAMETER(argv);

  SetConsoleOutputCP(CP_UTF8);
  setlocale(LC_ALL, ".UTF8");

  // Cast argv to UTF-8, if on UniversalCRT
#ifdef _UCRT
  int argc = 0;
  wchar_t** argv_unicode = CommandLineToArgvW(GetCommandLineW(), &argc);

  char** p_argv = calloc(argc, sizeof(char*));
  HANDLE hHeap = GetProcessHeap();
  for (int i = 0; i < argc; i++) {
    p_argv[i] = Win32W2U8(hHeap, argv_unicode[i]);
  }
  *argv = p_argv;
#endif
}
#else
#define FixWindowsUnicodeSupport(...) \
  do {                                \
  } while (false)
#endif
