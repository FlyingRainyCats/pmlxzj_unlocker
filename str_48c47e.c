#include <stdint.h>
#include <stdio.h>

#include <zlib.h>

uint8_t byte_48C47E[40] = {
    162, 162, 166, 218, 162, 227, 145, 172, 139, 243, 230, 139, 120, 153, 226, 130, 246, 225, 179, 81,
    80,  177, 186, 63,  108, 73,  191, 59,  109, 71,  70,  69,  69,  67,  66,  65,  160, 203, 44,  61,
};

void dump_byte_48C47E(void) {
  for (int i = 0; i < 40; i++) {
    byte_48C47E[i] ^= 100 - i;
  }
  FILE* f = fopen("dump.bin", "wb");
  fwrite(byte_48C47E, 1, sizeof(byte_48C47E), f);
  fclose(f);
}

void calc_start_offset(void) {
  int32_t value = (int32_t)0xFE28CDBB;
  value = -value;
  printf("new offset: 0x%08x\n", value);
}

int main() {
  //  dump_byte_48C47E();
  calc_start_offset();
  return 0;
}
