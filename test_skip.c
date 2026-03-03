#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int skipcomment (FILE *f, int *cp) {
  int c = *cp = getc(f);
  if (c == '#') {
    do {
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);
    return 1;
  }
  else return 0;
}

int main() {
    FILE *f = fopen("test_out.png", "rb");
    int c;
    if (skipcomment(f, &c)) printf("comment skipped\n");
    printf("c = %d (0x%X)\n", c, c);

    unsigned char png_sig[7] = {0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char check_sig[7] = {0};
    int read_bytes = fread(check_sig, 1, 7, f);
    printf("Read %d bytes\n", read_bytes);

    if (read_bytes == 7 && memcmp(check_sig, png_sig, 7) == 0) {
        printf("Signature matches!\n");
    } else {
        printf("Signature doesn't match! check_sig:\n");
        for(int i=0; i<7; i++) printf("%02X ", check_sig[i]);
        printf("\n");
        printf("png_sig:\n");
        for(int i=0; i<7; i++) printf("%02X ", png_sig[i]);
        printf("\n");
    }
    fclose(f);
    return 0;
}
