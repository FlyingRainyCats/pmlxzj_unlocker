#include <unistd.h>

#include "pmlxzj.h"
#include "pmlxzj_audio_aac.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_enum_names.h"

typedef struct {
  bool verbose;
  bool print_frames;
  bool print_indexes;
} pmlxzj_cmd_print_info_param_t;

pmlxzj_enumerate_state_e enum_frame_print_info(pmlxzj_state_t* app,
                                               pmlxzj_frame_info_t* frame,
                                               void* extra_callback_data) {
  pmlxzj_cmd_print_info_param_t* param = extra_callback_data;

  long frame_start_offset = ftell(app->file);
  if (frame->compressed_size > 10240) {
    long patch_start_offset = frame_start_offset + (long)frame->compressed_size / 2;
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x, patch=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset, (int)patch_start_offset);
  } else if (param->verbose) {
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset);
  }

  return PMLXZJ_ENUM_CONTINUE;
}

int pmlxzj_cmd_print_info(int argc, char** argv) {
  pmlxzj_cmd_print_info_param_t param = {0};
  int option = -1;
  while ((option = getopt(argc, argv, "vfi")) != -1) {
    switch (option) {
      case 'v':
        param.verbose = true;
        break;
      case 'f':
        param.print_frames = true;
        break;
      case 'i':
        param.print_indexes = true;
        break;
      default:
        fprintf(stderr, "ERROR: unknown option '-%c'\n", optopt);
        pmlxzj_usage(argv[0]);
        return 1;
    }
  }

  if (argc < optind + 1) {
    fprintf(stderr, "ERROR: 'input' required.\n");
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[optind], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  pmlxzj_state_t app = {0};
  pmlxzj_user_params_t params = {0};
  params.input_file = f_src;
  pmlxzj_state_e status = pmlxzj_init_all(&app, &params);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init pmlxzj failed: %d (%s)\n", status, pmlxzj_get_state_name(status));
    return 1;
  }
  printf("\n");

  if (param.print_indexes) {
    printf("idx1(len=%d):\n", (int)app.idx1_count);
    for (size_t i = 0; i < app.idx1_count; i++) {
      printf("  idx1[%3d]: %11d / 0x%08x\n", (int)i, app.idx1[i], app.idx1[i]);
    }
    printf("idx2(len=%d):\n", (int)app.idx2_count);
    for (size_t i = 0; i < 20; i++) {
      printf("  idx2[%3d]: %11d / 0x%08x\n", (int)i, app.idx2[i], app.idx2[i]);
    }
  }

  printf("offset: 0x%x\n", app.footer.offset_data_start);
  printf("frame:\n");
  printf("  offset: 0x%lx\n", app.first_frame_offset);
  printf("  type: %d\n", app.footer.config.image_compress_type);

  printf("audio:\n");
  printf("  offset: ");
  if (app.audio_metadata_offset) {
    printf("0x%08lx\n", app.audio_metadata_offset);
  } else {
    printf("(none)\n");
  }
  printf("  codec: %u  # %s\n", app.footer.config.audio_codec,
         pmlxzj_get_audio_codec_name(app.footer.config.audio_codec));

  switch (app.footer.config.audio_codec) {
    case PMLXZJ_AUDIO_TYPE_WAVE_RAW: {
      pmlxzj_audio_wav_t* audio = &app.audio.wav;
      printf("    offset: 0x%08lx\n", audio->offset);
      printf("    size:   0x%08x\n", audio->size);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_WAVE_COMPRESSED: {
      pmlxzj_audio_wav_zlib_t* audio = &app.audio.wav_zlib;
      printf("    offset:   0x%08lx\n", audio->offset);
      printf("    segments: 0x%08x\n", audio->segment_count);
      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_MP3: {
      pmlxzj_audio_mp3_t* audio = &app.audio.mp3;
      printf("    offset: 0x%08lx\n", audio->offset);
      printf("    size:   0x%08x\n", audio->size);
      if (param.verbose) {
        printf("    chunks:\n");
        for (uint32_t i = 0; i < audio->segment_count; i++) {
          printf("      mp3_chunk[%04u].offset: 0x%08x\n", i, audio->offsets[i]);
        }
      }

      break;
    }

    case PMLXZJ_AUDIO_TYPE_LOSSY_AAC: {
      pmlxzj_audio_aac_t* audio = &app.audio.aac;

      printf("    offset:   0x%08lx\n", audio->offset);
      printf("    size:     0x%08x\n", audio->size);
      printf("    segments: 0x%08x\n", audio->segment_count);
      printf("    format:   ");
      for (size_t i = 0; i < sizeof(audio->format); i++) {
        printf("%02x ", audio->format[i]);
      }
      printf("\n");
      printf("      profile:     %2d (%s)\n", audio->config.profile_id,
             pmlxzj_adts_get_profile_name(audio->config.profile_id));
      printf("      sample rate: %2d (%d Hz)\n", audio->config.sample_rate_id,
             pmlxzj_adts_get_sample_rate(audio->config.sample_rate_id));
      printf("      channel:     %2d\n", audio->config.channels_id);

      break;
    }
  }

  printf("encrypt edit_lock nonce: ");
  if (app.footer.edit_lock_nonce) {
    printf("0x%08x\n", app.footer.edit_lock_nonce);
  } else {
    printf("(unset)\n");
  }
  printf("encrypt play_lock password checksum: ");
  if (app.footer.play_lock_password_checksum) {
    printf("0x%08x\n", app.footer.play_lock_password_checksum);
  } else {
    printf("(unset)\n");
  }

  if (param.print_frames) {
    pmlxzj_enumerate_images(&app, enum_frame_print_info, &param);
  }
  fclose(f_src);

  return 0;
}
