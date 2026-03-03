#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
    FILE *f = fopen("test_out.png", "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    int img_width, img_height, img_comp;
    unsigned char *image_data = stbi_load_from_memory(data, size, &img_width, &img_height, &img_comp, 3); // STBI_rgb is 3
    printf("img_width=%d, img_height=%d, img_comp=%d, loaded=%d\n", img_width, img_height, img_comp, image_data!=NULL);

    if (image_data) {
        long expected_size = (long)img_width * img_height;
        printf("expected_size=%ld\n", expected_size);

        unsigned char *restored_data = malloc(expected_size);
        for (long i = 0; i < expected_size && i < 20; i++) {
            int x = i % img_width;
            int y = i / img_width;
            int img_idx = (y * img_width + x) * 3;
            int channel_index;
            int pos = i % 9;
            if (pos == 0) channel_index = 0;
            else if (pos == 1) channel_index = 1;
            else if (pos == 2) channel_index = 2;
            else if (pos == 3) channel_index = 2;
            else if (pos == 4) channel_index = 1;
            else if (pos == 5) channel_index = 0;
            else if (pos == 6) channel_index = 2;
            else if (pos == 7) channel_index = 1;
            else channel_index = 0;
            restored_data[i] = image_data[img_idx + channel_index] ^ 0x55;
            printf("%02X ", restored_data[i]);
        }
        printf("\n");
        printf("restored as char: %.*s\n", 20, restored_data);
    }

    return 0;
}
