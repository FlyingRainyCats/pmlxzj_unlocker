#include "pmlxzj.h"
#include "pmlxzj_commands.h"
#include "pmlxzj_utils.h"

#include <memory.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
  bool verbose;
  bool resume_on_bad_password;
  char password[21];
  FILE* file_destination;
} pmlxzj_cmd_unlock_exe_param_t;

pmlxzj_enumerate_state_e enum_frame_patch(pmlxzj_state_t* app, pmlxzj_frame_info_t* frame, void* extra) {
  pmlxzj_cmd_unlock_exe_param_t* cli_params = extra;
  long frame_start_offset = ftell(app->file);

  if (frame->compressed_size > 10240) {
    long key_pos = frame_start_offset + 4;
    long cipher_pos = frame_start_offset + (long)frame->compressed_size / 2;

    char frame_key[20];
    char frame_ciphered[20];
    fseek(app->file, key_pos, SEEK_SET);
    fread(frame_key, sizeof(frame_key), 1, app->file);
    fseek(app->file, cipher_pos, SEEK_SET);
    fread(frame_ciphered, sizeof(frame_ciphered), 1, app->file);

    for (int i = 0; i < 20; i++) {
      frame_ciphered[i] = (char)(frame_ciphered[i] ^ frame_key[i] ^ app->nonce_buffer[i]);
    }
    fseek(cli_params->file_destination, cipher_pos, SEEK_SET);
    fwrite(frame_ciphered, sizeof(frame_ciphered), 1, cli_params->file_destination);

    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x, patch=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset, (int)cipher_pos);
  } else if (cli_params->verbose) {
    printf("frame #%d, image #%d (len=(0x%x, 0x%x), offset=0x%x)\n", frame->frame_id, frame->image_id,
           frame->compressed_size, frame->decompressed_size, (int)frame_start_offset);
  }

  return PMLXZJ_ENUM_CONTINUE;
}

int pmlxzj_cmd_unlock_exe(int argc, char** argv) {
  pmlxzj_cmd_unlock_exe_param_t cli_params = {0};

  int opt;
  while ((opt = getopt(argc, argv, "vrp:P:")) != -1) {
    switch (opt) {
      case 'v':
        cli_params.verbose = true;
        break;

      case 'r':
        cli_params.resume_on_bad_password = true;
        break;

      case 'p':
        strncpy(cli_params.password, optarg, sizeof(cli_params.password) - 1);
        break;

      case 'P':
      {
        FILE* f_password = fopen(optarg, "rb");
        if (f_password == NULL) {
          perror("read password file");
          return 1;
        }
        fread(cli_params.password, sizeof(cli_params.password) - 1, 1, f_password);
        fclose(f_password);
        break;
      }

      default:
        fprintf(stderr, "ERROR: unknown option '-%c'\n", optopt);
        pmlxzj_usage(argv[0]);
        break;
    }
  }

  if (argc < optind + 2) {
    fprintf(stderr, "ERROR: missing parameters for input and output");
    pmlxzj_usage(argv[0]);
    return 1;
  }

  FILE* f_src = fopen(argv[optind], "rb");
  if (f_src == NULL) {
    perror("ERROR: open source input");
    return 1;
  }

  FILE* f_dst = fopen(argv[optind + 1], "wb");
  if (f_dst == NULL) {
    perror("ERROR: open dest input");
    fclose(f_src);
    return 1;
  }

  cli_params.file_destination = f_dst;

  pmlxzj_state_t app = {0};
  pmlxzj_user_params_t pmlxzj_param = {0};
  pmlxzj_param.input_file = f_src;
  pmlxzj_param.resume_on_bad_password = cli_params.resume_on_bad_password;
  memcpy(pmlxzj_param.password, cli_params.password, sizeof(pmlxzj_param.password));
  pmlxzj_state_e status = pmlxzj_init_all(&app, &pmlxzj_param);
  if (status != PMLXZJ_OK) {
    printf("ERROR: Init failed: %d\n", status);
    return 1;
  }

  pmlxzj_util_copy_file(f_dst, f_src);

  if (app.encrypt_mode == 1 || app.encrypt_mode == 2) {
    pmlxzj_enumerate_images(&app, enum_frame_patch, &cli_params);

    pmlxzj_footer_t new_footer = {0};
    memcpy(&new_footer, &app.footer, sizeof(new_footer));
    new_footer.edit_lock_nonce = 0;
    new_footer.play_lock_password_checksum = 0;
    fseek(f_dst, app.file_size - (long)(sizeof(pmlxzj_footer_t)), SEEK_SET);
    fwrite(&new_footer, sizeof(new_footer), 1, f_dst);
  } else {
    printf("error: edit_lock_nonce is zero. Unsupported cipher or not encrypted.\n");
  }

  fclose(f_src);
  fclose(f_dst);

  return 0;
}
