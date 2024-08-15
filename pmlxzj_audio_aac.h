#pragma once

#include "pmlxzj.h"

#include <stddef.h>
#include <stdint.h>

#define PMLXZJ_AAC_ADTS_HEADER_LEN (7)
#define PMLXZJ_AAC_MASK_BY_BITS(val, n) ((val) & ((1 << n) - 1))
#define PMLXZJ_AAC_MAX_FRAME_DATA_LEN ((1 << 13) - 1 - PMLXZJ_AAC_ADTS_HEADER_LEN)

inline static uint8_t pmlxzj_aac_map_profile_to_adts(uint8_t profile) {
  switch (profile) {
    case 1:  // AAC Main
      return 0;
    case 2:  // AAC LC
      return 1;
    case 3:  // AAC SSR
      return 2;
    default:  // (5) SBR or other: HE?
      return 3;
  }
}
inline static uint8_t pmlxzj_aac_map_sample_rate_idx(uint8_t sample_rate_idx) {
  // Sample rate index is (0..11) inclusive.
  if (sample_rate_idx < 12) {
    return sample_rate_idx;
  }

  return 0xFF;
}

inline static pmlxzj_state_e pmlxzj_aac_parse_decoder_specific_info(pmlxzj_aac_audio_config_t* result,
                                                                    const uint8_t* buf,
                                                                    size_t len) {
  if (len < 2) {
    return PMLXZJ_AUDIO_AAC_DECODER_INFO_TOO_SMALL;
  }

  const uint8_t profile = PMLXZJ_AAC_MASK_BY_BITS(buf[0] >> 3, 5);
  const uint8_t sample_rate = PMLXZJ_AAC_MASK_BY_BITS((buf[0] << 1) | (buf[1] >> 7), 4);
  const uint8_t channels = PMLXZJ_AAC_MASK_BY_BITS(buf[1] >> 3, 4);

  if (sample_rate == 0xFF) {
    return PMLXZJ_AUDIO_AAC_DECODER_UNSUPPORTED_SAMPLE_RATE;
  }

  result->profile = pmlxzj_aac_map_profile_to_adts(profile);
  result->sample_rate_idx = sample_rate;
  result->channels_idx = channels;
  return PMLXZJ_OK;
}

// header should have size of 9 or more to be safe.
// returns the number of bytes required by the header.
// `frame_data_len` should be `PMLXZJ_AAC_MAX_FRAME_DATA_LEN` or lower.
inline static size_t pmlxzj_aac_adts_header(uint8_t* header,
                                            const pmlxzj_aac_audio_config_t* config,
                                            uint16_t frame_data_len) {
  header[0] = 0xff;  // sync word (12-bits)

  // sync word: 4-bits
  const uint8_t mpeg_version = PMLXZJ_AAC_MASK_BY_BITS(1, 1);        // 1-bit
  const uint8_t layer = PMLXZJ_AAC_MASK_BY_BITS(0, 2);               // 2-bits
  const uint8_t protection_absence = PMLXZJ_AAC_MASK_BY_BITS(1, 1);  // 1-bit
  header[1] = 0xf0 | (mpeg_version << 3) | (layer << 1) | protection_absence;

  const uint8_t profile = config->profile;                                                     // 2-bits
  const uint8_t sample_rate_idx_mapped = PMLXZJ_AAC_MASK_BY_BITS(config->sample_rate_idx, 4);  // 4-bits
  const uint8_t private_bit = 0;                                                               // 1-bit
  const uint8_t channels_idx = PMLXZJ_AAC_MASK_BY_BITS(config->channels_idx, 3);               // 3-bits (1 + 2)
  header[2] = (profile << 6) | (sample_rate_idx_mapped << 2) | (private_bit << 1) | (channels_idx >> 2);

  // channel (2-bits)
  const uint8_t originality = PMLXZJ_AAC_MASK_BY_BITS(0, 1);      // 1-bit
  const uint8_t home_usage = PMLXZJ_AAC_MASK_BY_BITS(0, 1);       // 1-bit
  const uint8_t copyrighted = PMLXZJ_AAC_MASK_BY_BITS(0, 1);      // 1-bit
  const uint8_t copyright_start = PMLXZJ_AAC_MASK_BY_BITS(0, 1);  // 1-bit
  const uint16_t frame_size =
      PMLXZJ_AAC_MASK_BY_BITS(frame_data_len + PMLXZJ_AAC_ADTS_HEADER_LEN, 13);  // 13-bits (2 + 8 + 3)
  header[3] = (channels_idx << 6) | (originality << 5) | (home_usage << 4)       //
              | (copyrighted << 3) | (copyright_start << 2) | (frame_size >> 11);
  header[4] = frame_size >> 3;

  // frame_size (3-bits)
  const uint16_t buffer_fullness = PMLXZJ_AAC_MASK_BY_BITS(0x7FF, 11);  // 11-bits (5 + 6)
  header[5] = (uint8_t)((frame_size << 5) | (buffer_fullness >> 6));

  // buffer_fullness (6-bits)
  const uint8_t number_of_aac_frames = PMLXZJ_AAC_MASK_BY_BITS(0, 2);  // 2-bits
  header[6] = (uint8_t)(buffer_fullness << 2) | (number_of_aac_frames);

  return 7;
}
