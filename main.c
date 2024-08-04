#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmlxzj.h"

void usage(char* argv0) {
  printf("Usage: %s audio-dump <input> <output_audio>\n", argv0);
  printf("Usage: %s unlock <input> <output>\n", argv0);
  printf("Usage: %s info <input>\n", argv0);
}

#define PL_MIN(a, b) (((a) < (b)) ? (a) : (b))

int extract_audio(int argc, char** argv) {
  if (argc <= 3) {
    usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[2], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_state_e status = pmlxzj_init(&app, f_src);
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

int unlock_exe_player(int argc, char** argv) {
  if (argc <= 3) {
    usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[2], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  FILE* f_dst = fopen(argv[3], "wb");
  if (f_dst == NULL) {
    perror("ERROR: open dest input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_state_e status = pmlxzj_init(&app, f_src);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj exe failed: %d\n", status);
    return 1;
  }
  status = pmlxzj_init_audio(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (audio) failed: %d\n", status);
    return 1;
  }
  status = pmlxzj_init_frame(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (frame) failed: %d\n", status);
    return 1;
  }

  // Read entire exe to memory
  fseek(f_src, 0, SEEK_SET);
  {
    size_t bytes_left = app.file_size;
    char* buff_exe = malloc(4096);

    while (true) {
      size_t bytes_read = fread(buff_exe, 1, PL_MIN(4096, bytes_left), f_src);
      if (bytes_read == 0)
        break;
      bytes_left -= bytes_read;
      fwrite(buff_exe, 1, bytes_read, f_dst);
    }

    assert(bytes_left == 0 && "not all bytes read");

    free(buff_exe);
  }

  printf("offset_data_start: 0x%x\n", app.footer.offset_data_start);
  printf("frame offset: 0x%lx\n", app.first_frame_offset);
  printf("audio offset: 0x%lx\n", app.audio_start_offset);

  fseek(f_src, app.first_frame_offset, SEEK_SET);

  bool f24 = app.field_14d8.field_24 == 1;

  // special case: first frame does not come with rect set.
  {
    pmlxzj_try_fix_frame(&app, f_dst);

    fseek(f_src, 4, SEEK_CUR);  // int after frame
    if (f24) {
      fseek(f_src, 8, SEEK_CUR);  // f24: timestamps?
    }
  }

  long pos = 0;

  bool read_all_frames = false;
  while (!read_all_frames && pos < app.audio_start_offset) {
    pos = ftell(f_src);

    // setup stream2 data, when f24 is set
    if (f24) {  // seek stream2
      uint32_t stream2_len = 0;
      fread(&stream2_len, sizeof(stream2_len), 1, f_src);
      if (stream2_len != 0) {
        fseek(f_src, (long)stream2_len, SEEK_CUR);
      }
    }

    int32_t frame_prop = 0;  // anything other than 0
    fread(&frame_prop, sizeof(frame_prop), 1, f_src);

    // draw patch
    while (frame_prop > 0 && pos < app.audio_start_offset) {
      struct {
        // don't care about where it renders.
        uint32_t patch_rect[4];

        uint32_t compressed_size;
        int32_t decompressed_size;
        int32_t end_of_frame_mark;
      } frame_hdr;
      fread(&frame_hdr, sizeof(frame_hdr), 1, f_src);
      if (frame_hdr.decompressed_size == -1 && frame_hdr.end_of_frame_mark == -1) {
        printf("reach read_all_frames of frame mark, offset = %08x\n", (int)(ftell(f_src)));
        read_all_frames = true;
        break;
      }

      fseek(f_src, -12, SEEK_CUR);
      pmlxzj_try_fix_frame(&app, f_dst);

      fread(&frame_prop, sizeof(frame_prop), 1, f_src);
      if (feof(f_src)) {
        printf("WARN: unexpected eof, offset calc wrong?\n");
        read_all_frames = true;
        break;
      }
    }

    if (f24) {
      fseek(f_src, 8, SEEK_CUR);
    }
  }

  // nonce_key_mode_1
  {
    uint32_t nonce_key_mode_1 = 0;
    fseek(f_dst, app.file_size - 0x2C, SEEK_SET);
    fwrite(&nonce_key_mode_1, sizeof(nonce_key_mode_1), 1, f_dst);
  }

  return 0;
}

int dump_info(int argc, char** argv) {
  if (argc <= 2) {
    usage(argv[0]);
    return 1;
  }
  FILE* f_src = fopen(argv[2], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_state_e status = pmlxzj_init(&app, f_src);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj exe failed: %d\n", status);
    return 1;
  }
  status = pmlxzj_init_audio(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (audio) failed: %d\n", status);
    return 1;
  }
  status = pmlxzj_init_frame(&app);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj (frame) failed: %d\n", status);
    return 1;
  }

  printf("idx1(len=%d):\n", (int)app.idx1_count);
  for (size_t i = 0; i < app.idx1_count; i++) {
    printf("  idx1[%3d]: %11d / 0x%08x\n", (int)i, app.idx1[i], app.idx1[i]);
  }
  printf("idx2(len=%d):\n", (int)app.idx2_count);
  for (size_t i = 0; i < 20; i++) {
    printf("  idx2[%3d]: %11d / 0x%08x\n", (int)i, app.idx2[i], app.idx2[i]);
  }

  printf("offset_data_start: 0x%x\n", app.footer.offset_data_start);
  printf("frame offset: 0x%lx\n", app.first_frame_offset);
  printf("audio offset: 0x%lx\n", app.audio_start_offset);

  fseek(f_src, app.first_frame_offset, SEEK_SET);

  bool f24 = app.field_14d8.field_24 == 1;

  size_t frame = 0;
  // special case: first frame does not come with rect set.
  {
    uint32_t compressed_size = 0;
    fread(&compressed_size, sizeof(uint32_t), 1, f_src);
    printf("frame %d (len=0x%x, offset=0x%x)\n", (int)frame, compressed_size, (int)ftell(f_src));
    frame++;
    fseek(f_src, (long)compressed_size, SEEK_CUR);
    fseek(f_src, 4, SEEK_CUR);  // int after frame
    if (f24) {
      fseek(f_src, 8, SEEK_CUR);  // f24: timestamps?
    }
  }

  long pos = 0;

  bool read_all_frames = false;
  while (!read_all_frames && pos < app.audio_start_offset) {
    pos = ftell(f_src);
#ifndef NDEBUG
    printf("pos = 0x%x\n", (int)pos);
    fflush(stdout);
#endif

    // setup stream2 data, when f24 is set
    if (f24) {  // seek stream2
      uint32_t stream2_len = 0;
      fread(&stream2_len, sizeof(stream2_len), 1, f_src);
      if (stream2_len != 0) {
        fseek(f_src, (long)stream2_len, SEEK_CUR);
      }
    }

    int32_t frame_prop = 0;  // anything other than 0
    fread(&frame_prop, sizeof(frame_prop), 1, f_src);

    // draw patch
    while (frame_prop > 0 && pos < app.audio_start_offset) {
      struct {
        // don't care about where it renders.
        uint32_t patch_rect[4];

        uint32_t compressed_size;
        int32_t decompressed_size;
        int32_t end_of_frame_mark;
      } frame_hdr;
      fread(&frame_hdr, sizeof(frame_hdr), 1, f_src);
      if (frame_hdr.decompressed_size == -1 && frame_hdr.end_of_frame_mark == -1) {
        printf("reach read_all_frames of frame mark, offset = %08x\n", (int)(ftell(f_src)));
        read_all_frames = true;
        break;
      }
      if (frame_hdr.compressed_size > 10240) {
        long frame_start_offset = ftell(f_src) - 12;  // at compressed_size
        long patch_start_offset = frame_start_offset + 4 + (long)frame_hdr.compressed_size / 2;
        printf("frame %d (len=(0x%x, 0x%x), offset=0x%x, patch=0x%x)\n", (int)frame, frame_hdr.compressed_size,
               frame_hdr.decompressed_size, (int)frame_start_offset, (int)patch_start_offset);
      }
#ifndef NDEBUG
      fflush(stdout);
#endif
      frame++;
      // we read 8 bytes ahead, so remove the 8 byte offset here.
      fseek(f_src, (long)frame_hdr.compressed_size - 8, SEEK_CUR);
      fread(&frame_prop, sizeof(frame_prop), 1, f_src);

      if (feof(f_src)) {
        printf("WARN: unexpected eof, offset calc wrong?\n");
        read_all_frames = true;
        break;
      }
    }

    if (f24) {
      fseek(f_src, 8, SEEK_CUR);
    }
  }

  return 0;
}

int main(int argc, char** argv) {
  printf("pmlxzj_unlocker by AiFeiDeMao@52pojie.cn (FlyingRainyCats)\n");
  if (argc <= 2) {
    usage(argv[0]);
    return 1;
  }

  if (strcmpi(argv[1], "audio-dump") == 0) {
    return extract_audio(argc, argv);
  } else if (strcmpi(argv[1], "unlock") == 0) {
    return unlock_exe_player(argc, argv);
  } else if (strcmpi(argv[1], "info") == 0) {
    return dump_info(argc, argv);
  }

  usage(argv[0]);
  return 1;
}
