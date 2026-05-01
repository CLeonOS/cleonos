#ifndef CLEONOS_UWM_PSF_FONT_H
#define CLEONOS_UWM_PSF_FONT_H

#include <cleonos_syscall.h>

#define UWM_PSF_FONT_PATH "/system/tty.psf"
#define UWM_PSF_MAX_BYTES 65536ULL
#define UWM_PSF2_MAGIC 0x864AB572U
#define UWM_PSF2_HEADER_MIN 32U

typedef struct uwm_psf_font_cache {
    int tried;
    int ready;
    unsigned char blob[UWM_PSF_MAX_BYTES];
    u64 size;
    unsigned int headersize;
    unsigned int length;
    unsigned int charsize;
    unsigned int width;
    unsigned int height;
    unsigned int bytes_per_row;
} uwm_psf_font_cache;

static uwm_psf_font_cache uwm_psf_font;

static unsigned int uwm_psf_read_u32_le(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U) | ((unsigned int)ptr[2] << 16U) |
           ((unsigned int)ptr[3] << 24U);
}

static int uwm_psf_try_load(void) {
    u64 size;
    u64 got;
    unsigned int magic;
    unsigned int headersize;
    unsigned int length;
    unsigned int charsize;
    unsigned int height;
    unsigned int width;
    unsigned int bytes_per_row;
    u64 payload_bytes;

    if (uwm_psf_font.tried != 0) {
        return uwm_psf_font.ready;
    }

    uwm_psf_font.tried = 1;
    uwm_psf_font.ready = 0;

    size = cleonos_sys_fs_stat_size(UWM_PSF_FONT_PATH);
    if (size < UWM_PSF2_HEADER_MIN || size > UWM_PSF_MAX_BYTES) {
        return 0;
    }

    got = cleonos_sys_fs_read(UWM_PSF_FONT_PATH, (char *)uwm_psf_font.blob, size);
    if (got != size) {
        return 0;
    }

    magic = uwm_psf_read_u32_le(uwm_psf_font.blob + 0U);
    headersize = uwm_psf_read_u32_le(uwm_psf_font.blob + 8U);
    length = uwm_psf_read_u32_le(uwm_psf_font.blob + 16U);
    charsize = uwm_psf_read_u32_le(uwm_psf_font.blob + 20U);
    height = uwm_psf_read_u32_le(uwm_psf_font.blob + 24U);
    width = uwm_psf_read_u32_le(uwm_psf_font.blob + 28U);

    if (magic != UWM_PSF2_MAGIC || headersize < UWM_PSF2_HEADER_MIN || (u64)headersize > size || length == 0U ||
        charsize == 0U || width == 0U || height == 0U) {
        return 0;
    }

    bytes_per_row = (width + 7U) / 8U;
    if (bytes_per_row == 0U || ((u64)bytes_per_row * (u64)height) > (u64)charsize) {
        return 0;
    }

    if ((u64)length > ((size - (u64)headersize) / (u64)charsize)) {
        return 0;
    }

    payload_bytes = (u64)length * (u64)charsize;
    if (payload_bytes > (size - (u64)headersize)) {
        return 0;
    }

    uwm_psf_font.size = size;
    uwm_psf_font.headersize = headersize;
    uwm_psf_font.length = length;
    uwm_psf_font.charsize = charsize;
    uwm_psf_font.width = width;
    uwm_psf_font.height = height;
    uwm_psf_font.bytes_per_row = bytes_per_row;
    uwm_psf_font.ready = 1;
    return 1;
}

static int uwm_psf_ready(void) {
    return uwm_psf_try_load();
}

static int uwm_psf_source_step(int scale) {
    return (scale <= 1) ? 2 : 1;
}

static int uwm_psf_pixel_scale(int scale) {
    return (scale <= 1) ? 1 : (scale - 1);
}

static int uwm_psf_draw_width(int scale) {
    int step;
    int pixel_scale;

    if (uwm_psf_ready() == 0) {
        return 5 * scale;
    }

    step = uwm_psf_source_step(scale);
    pixel_scale = uwm_psf_pixel_scale(scale);
    return (int)(((uwm_psf_font.width + (unsigned int)step - 1U) / (unsigned int)step) *
                 (unsigned int)pixel_scale);
}

static int uwm_psf_draw_height(int scale) {
    int step;
    int pixel_scale;

    if (uwm_psf_ready() == 0) {
        return 7 * scale;
    }

    step = uwm_psf_source_step(scale);
    pixel_scale = uwm_psf_pixel_scale(scale);
    return (int)(((uwm_psf_font.height + (unsigned int)step - 1U) / (unsigned int)step) *
                 (unsigned int)pixel_scale);
}

static int uwm_psf_advance(int scale) {
    int width = uwm_psf_draw_width(scale);
    int pixel_scale = uwm_psf_pixel_scale(scale);

    if (uwm_psf_ready() == 0) {
        return 6 * scale;
    }

    return width + pixel_scale;
}

static const unsigned char *uwm_psf_glyph(char ch) {
    unsigned int code;

    if (uwm_psf_ready() == 0) {
        return (const unsigned char *)0;
    }

    code = (unsigned int)(unsigned char)ch;
    if (code >= uwm_psf_font.length) {
        code = (unsigned int)'?';
        if (code >= uwm_psf_font.length) {
            code = 0U;
        }
    }

    return uwm_psf_font.blob + uwm_psf_font.headersize + ((u64)code * (u64)uwm_psf_font.charsize);
}

static int uwm_psf_glyph_bit(const unsigned char *glyph, unsigned int row, unsigned int col) {
    const unsigned char *row_bits;
    unsigned char byte;
    unsigned int bit;

    if (glyph == (const unsigned char *)0 || row >= uwm_psf_font.height || col >= uwm_psf_font.width) {
        return 0;
    }

    row_bits = glyph + ((u64)row * (u64)uwm_psf_font.bytes_per_row);
    byte = row_bits[col / 8U];
    bit = 0x80U >> (col & 7U);
    return ((unsigned int)byte & bit) != 0U ? 1 : 0;
}

static int uwm_psf_glyph_block_any(const unsigned char *glyph, unsigned int row, unsigned int col, int step) {
    int dy;

    if (step <= 1) {
        return uwm_psf_glyph_bit(glyph, row, col);
    }

    for (dy = 0; dy < step; dy++) {
        int dx;
        unsigned int sy = row + (unsigned int)dy;

        if (sy >= uwm_psf_font.height) {
            break;
        }

        for (dx = 0; dx < step; dx++) {
            unsigned int sx = col + (unsigned int)dx;

            if (sx >= uwm_psf_font.width) {
                break;
            }

            if (uwm_psf_glyph_bit(glyph, sy, sx) != 0) {
                return 1;
            }
        }
    }

    return 0;
}

#endif
