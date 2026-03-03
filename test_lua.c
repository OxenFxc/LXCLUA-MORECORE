#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
    FILE *f = fopen("test_out.png", "rb");
    fseek(f, 0, SEEK_END);
    long png_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *png_data = malloc(png_size);
    fread(png_data, 1, png_size, f);
    fclose(f);

    int img_width, img_height, img_comp;
    unsigned char *image_data = stbi_load_from_memory(png_data, (int)png_size, &img_width, &img_height, &img_comp, 3);
    if (!image_data) {
        printf("Decode failed: %s\n", stbi_failure_reason());
    } else {
        printf("Decode success!\n");
    }
    return 0;
}
