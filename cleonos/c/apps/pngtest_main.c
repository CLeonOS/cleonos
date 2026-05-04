#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

typedef struct pngtest_mem_reader {
    const unsigned char *data;
    size_t size;
    size_t offset;
    int failed;
} pngtest_mem_reader;

static const unsigned char pngtest_rgba2x2[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D,
    0x24, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x0C, 0x81, 0x34, 0x18, 0x00, 0x00, 0x49,
    0xC8, 0x09, 0xF7, 0xF9, 0xAB, 0xB6, 0x0D, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
    0x42, 0x60, 0x82,
};

static void pngtest_read_cb(png_structp png_ptr, png_bytep out, size_t length) {
    pngtest_mem_reader *reader = (pngtest_mem_reader *)png_get_io_ptr(png_ptr);

    if (reader == NULL || out == NULL || reader->offset > reader->size || length > reader->size - reader->offset) {
        if (reader != NULL) {
            reader->failed = 1;
        }
        png_error(png_ptr, "pngtest read overflow");
        return;
    }

    memcpy(out, reader->data + reader->offset, length);
    reader->offset += length;
}

static void pngtest_warn_cb(png_structp png_ptr, png_const_charp message) {
    (void)png_ptr;
    printf("pngtest: warning: %s\n", message != NULL ? message : "(null)");
}

static int pngtest_decode_rgba(void) {
    pngtest_mem_reader reader;
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace_type = 0;
    int compression_type = 0;
    int filter_type = 0;
    size_t rowbytes;
    png_bytep rows[2];
    unsigned char pixels[2 * 2 * 4];
    static const unsigned char expected[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255,
    };

    memset(&reader, 0, sizeof(reader));
    reader.data = pngtest_rgba2x2;
    reader.size = sizeof(pngtest_rgba2x2);

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, pngtest_warn_cb);
    if (png_ptr == NULL) {
        puts("pngtest: png_create_read_struct failed");
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        puts("pngtest: png_create_info_struct failed");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 0;
    }

    png_set_read_fn(png_ptr, &reader, pngtest_read_cb);
    png_read_info(png_ptr, info_ptr);

    if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                     &interlace_type, &compression_type, &filter_type) == 0) {
        puts("pngtest: png_get_IHDR failed");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    printf("pngtest: %llux%llu bit=%d color=%d interlace=%d\n", (unsigned long long)width,
           (unsigned long long)height, bit_depth, color_type, interlace_type);

    if (width != 2 || height != 2 || bit_depth != 8 || color_type != PNG_COLOR_TYPE_RGBA) {
        puts("pngtest: unexpected IHDR");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    png_read_update_info(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes != 8) {
        printf("pngtest: unexpected rowbytes=%llu\n", (unsigned long long)rowbytes);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    rows[0] = pixels;
    rows[1] = pixels + rowbytes;
    memset(pixels, 0, sizeof(pixels));
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (reader.failed != 0 || memcmp(pixels, expected, sizeof(expected)) != 0) {
        puts("pngtest: pixel mismatch");
        return 0;
    }

    puts("pngtest: decoded RGBA pixels OK");
    return 1;
}

int cleonos_app_main(void) {
    printf("pngtest: libpng %s\n", png_get_libpng_ver(NULL));

    if (pngtest_decode_rgba() == 0) {
        puts("pngtest: FAIL");
        return 1;
    }

    puts("pngtest: PASS");
    return 0;
}
