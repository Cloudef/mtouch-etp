#include <stdint.h>
#include <stdio.h>
#include <string.h>

void main()
{
   uint8_t bytes[960];
   FILE *f = fopen("raw.bin", "rb");
   fread(bytes, 1, sizeof(bytes), f);
   
   for (size_t i = 0; i < sizeof(bytes); ++i) {
      if (bytes[i] == 0x81) {
	 uint8_t pos[4];
         memcpy(pos, bytes + i + 1, sizeof(pos));
	 printf("X: %u Y: %u\n", pos[0] * 0x80 + pos[1], pos[2] * 0x80 + pos[3]);
         i += 4;
      }
   }

}
