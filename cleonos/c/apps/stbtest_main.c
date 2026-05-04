#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb/stb_image_cleonos.h"

static const unsigned char stbtest_tga2x2[] = {
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00,
    0x18, 0x20,
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
    0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
};

static int stbtest_builtin(void) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *rgba;
    static const unsigned char expected[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };

    rgba = stbi_load_from_memory(stbtest_tga2x2, (int)sizeof(stbtest_tga2x2), &width, &height, &channels, 4);
    if (rgba == NULL) {
        printf("stbtest: built-in decode failed: %s\n", stbi_failure_reason());
        return 0;
    }

    printf("stbtest: built-in TGA %dx%d channels=%d\n", width, height, channels);
    if (width != 2 || height != 2 || memcmp(rgba, expected, sizeof(expected)) != 0) {
        stbi_image_free(rgba);
        puts("stbtest: pixel mismatch");
        return 0;
    }

    stbi_image_free(rgba);
    return 1;
}

int cleonos_app_main(void) {
    if (stbtest_builtin() == 0) {
        puts("stbtest: FAIL");
        return 1;
    }

    puts("stbtest: PASS");
    return 0;
}
