#include "cmd_runtime.h"
#include <stdio.h>

#define USH_BMPVIEW_DEFAULT_COLS 80ULL
#define USH_BMPVIEW_MAX_COLS 160ULL
#define USH_BMPVIEW_MAX_ROWS 120ULL
#define USH_BMPVIEW_MAX_IMAGE_WIDTH 4096ULL
#define USH_BMPVIEW_MAX_IMAGE_HEIGHT 4096ULL
#define USH_BMPVIEW_MAX_ROW_BYTES 16384U

typedef struct ush_bmpview_info {
    u64 width;
    u64 height;
    u64 bytes_per_pixel;
    u64 row_stride;
    u64 pixel_offset;
    int top_down;
} ush_bmpview_info;

static unsigned char ush_bmpview_row_buf[USH_BMPVIEW_MAX_ROW_BYTES];
static char ush_bmpview_render[USH_BMPVIEW_MAX_ROWS][USH_BMPVIEW_MAX_COLS + 1U];
static u64 ush_bmpview_sample_rows[USH_BMPVIEW_MAX_ROWS];

static unsigned int ush_bmpview_le16(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U);
}

static unsigned int ush_bmpview_le32(const unsigned char *ptr) {
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8U) | ((unsigned int)ptr[2] << 16U) |
           ((unsigned int)ptr[3] << 24U);
}

static int ush_bmpview_le32s(const unsigned char *ptr) {
    return (int)((unsigned int)ush_bmpview_le32(ptr));
}

static int ush_bmpview_read_exact(u64 fd, unsigned char *out, u64 size) {
    u64 done = 0ULL;

    if (out == (unsigned char *)0 || size == 0ULL) {
        return 0;
    }

    while (done < size) {
        u64 got = cleonos_sys_fd_read(fd, out + done, size - done);

        if (got == (u64)-1 || got == 0ULL) {
            return 0;
        }

        done += got;
    }

    return 1;
}

static int ush_bmpview_skip_bytes(u64 fd, u64 size) {
    unsigned char scratch[256];
    u64 left = size;

    while (left > 0ULL) {
        u64 want = left;
        u64 got;

        if (want > (u64)sizeof(scratch)) {
            want = (u64)sizeof(scratch);
        }

        got = cleonos_sys_fd_read(fd, scratch, want);

        if (got == (u64)-1 || got == 0ULL) {
            return 0;
        }

        left -= got;
    }

    return 1;
}

static int ush_bmpview_parse_header(u64 fd, ush_bmpview_info *out_info) {
    unsigned char header[54];
    unsigned int dib_size;
    int width_s;
    int height_s;
    unsigned int planes;
    unsigned int bits_per_pixel;
    unsigned int compression;
    u64 width_u;
    u64 height_u;
    u64 row_stride;
    u64 bytes_per_pixel;

    if (out_info == (ush_bmpview_info *)0) {
        return 0;
    }

    if (ush_bmpview_read_exact(fd, header, (u64)sizeof(header)) == 0) {
        return 0;
    }

    if (header[0] != (unsigned char)'B' || header[1] != (unsigned char)'M') {
        return 0;
    }

    dib_size = ush_bmpview_le32(&header[14]);
    if (dib_size < 40U) {
        return 0;
    }

    width_s = ush_bmpview_le32s(&header[18]);
    height_s = ush_bmpview_le32s(&header[22]);
    planes = ush_bmpview_le16(&header[26]);
    bits_per_pixel = ush_bmpview_le16(&header[28]);
    compression = ush_bmpview_le32(&header[30]);

    if (planes != 1U) {
        return 0;
    }

    if (width_s <= 0 || height_s == 0) {
        return 0;
    }

    if (bits_per_pixel != 24U && bits_per_pixel != 32U) {
        return 0;
    }

    if (compression != 0U) {
        return 0;
    }

    width_u = (u64)(unsigned int)width_s;
    if (height_s < 0) {
        i64 signed_h = (i64)height_s;
        height_u = (u64)(-signed_h);
        out_info->top_down = 1;
    } else {
        height_u = (u64)(unsigned int)height_s;
        out_info->top_down = 0;
    }

    if (width_u == 0ULL || height_u == 0ULL) {
        return 0;
    }

    if (width_u > USH_BMPVIEW_MAX_IMAGE_WIDTH || height_u > USH_BMPVIEW_MAX_IMAGE_HEIGHT) {
        return 0;
    }

    bytes_per_pixel = (u64)(bits_per_pixel / 8U);
    row_stride = (width_u * bytes_per_pixel + 3ULL) & ~3ULL;

    if (row_stride == 0ULL || row_stride > (u64)USH_BMPVIEW_MAX_ROW_BYTES) {
        return 0;
    }

    out_info->pixel_offset = (u64)ush_bmpview_le32(&header[10]);
    if (out_info->pixel_offset < (u64)sizeof(header)) {
        return 0;
    }

    out_info->width = width_u;
    out_info->height = height_u;
    out_info->bytes_per_pixel = bytes_per_pixel;
    out_info->row_stride = row_stride;
    return 1;
}

static char ush_bmpview_luma_char(unsigned int r, unsigned int g, unsigned int b) {
    static const char ramp[] = " .:-=+*#%@";
    unsigned int luma = (77U * r + 150U * g + 29U * b) >> 8U;
    unsigned int idx = (luma * 9U) / 255U;
    return ramp[idx];
}

static int ush_bmpview_parse_args(const char *arg, char *out_path, u64 out_path_size, u64 *out_cols) {
    char first[USH_PATH_MAX];
    char second[32];
    const char *rest = "";
    const char *rest2 = "";

    if (out_path == (char *)0 || out_cols == (u64 *)0 || out_path_size == 0ULL) {
        return 0;
    }

    *out_cols = USH_BMPVIEW_DEFAULT_COLS;
    out_path[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    ush_copy(out_path, out_path_size, first);

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }

        if (ush_parse_u64_dec(second, out_cols) == 0 || *out_cols == 0ULL) {
            return 0;
        }

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            return 0;
        }
    }

    return 1;
}

static int ush_cmd_bmpview(const ush_state *sh, const char *arg) {
    ush_bmpview_info info;
    char path_arg[USH_PATH_MAX];
    char abs_path[USH_PATH_MAX];
    u64 cols = USH_BMPVIEW_DEFAULT_COLS;
    u64 out_w;
    u64 out_h;
    u64 file_row;
    u64 fd;
    int parse_ret;

    if (sh == (const ush_state *)0) {
        return 0;
    }

    parse_ret = ush_bmpview_parse_args(arg, path_arg, (u64)sizeof(path_arg), &cols);
    if (parse_ret == 2) {
        ush_writeln("usage: bmpview <file.bmp> [cols]");
        ush_writeln("note: supports uncompressed 24/32-bit BMP");
        return 1;
    }

    if (parse_ret == 0) {
        ush_writeln("bmpview: usage bmpview <file.bmp> [cols]");
        return 0;
    }

    if (ush_resolve_path(sh, path_arg, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln("bmpview: invalid path");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(abs_path) != 1ULL) {
        ush_writeln("bmpview: file not found");
        return 0;
    }

    fd = cleonos_sys_fd_open(abs_path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        ush_writeln("bmpview: open failed");
        return 0;
    }

    ush_zero(&info, (u64)sizeof(info));
    if (ush_bmpview_parse_header(fd, &info) == 0) {
        (void)cleonos_sys_fd_close(fd);
        ush_writeln("bmpview: unsupported or invalid bmp");
        return 0;
    }

    if (info.pixel_offset > 54ULL) {
        if (ush_bmpview_skip_bytes(fd, info.pixel_offset - 54ULL) == 0) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("bmpview: failed to seek pixel data");
            return 0;
        }
    }

    if (cols > USH_BMPVIEW_MAX_COLS) {
        cols = USH_BMPVIEW_MAX_COLS;
    }
    if (cols > info.width) {
        cols = info.width;
    }
    if (cols == 0ULL) {
        cols = 1ULL;
    }

    out_w = cols;
    out_h = (info.height * out_w) / (info.width * 2ULL);
    if (out_h == 0ULL) {
        out_h = 1ULL;
    }
    if (out_h > USH_BMPVIEW_MAX_ROWS) {
        out_h = USH_BMPVIEW_MAX_ROWS;
    }
    if (out_h > info.height) {
        out_h = info.height;
    }

    {
        u64 oy;
        u64 ox;

        for (oy = 0ULL; oy < out_h; oy++) {
            ush_bmpview_sample_rows[oy] = (oy * info.height) / out_h;
            for (ox = 0ULL; ox < out_w; ox++) {
                ush_bmpview_render[oy][ox] = ' ';
            }
            ush_bmpview_render[oy][out_w] = '\0';
        }
    }

    for (file_row = 0ULL; file_row < info.height; file_row++) {
        u64 display_row;
        u64 oy;

        if (ush_bmpview_read_exact(fd, ush_bmpview_row_buf, info.row_stride) == 0) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln("bmpview: read pixel data failed");
            return 0;
        }

        if (info.top_down != 0) {
            display_row = file_row;
        } else {
            display_row = (info.height - 1ULL) - file_row;
        }

        for (oy = 0ULL; oy < out_h; oy++) {
            u64 ox;

            if (ush_bmpview_sample_rows[oy] != display_row) {
                continue;
            }

            for (ox = 0ULL; ox < out_w; ox++) {
                u64 src_x = (ox * info.width) / out_w;
                u64 pix_off = src_x * info.bytes_per_pixel;
                unsigned int b;
                unsigned int g;
                unsigned int r;

                if (pix_off + 2ULL >= info.row_stride) {
                    ush_bmpview_render[oy][ox] = ' ';
                    continue;
                }

                b = (unsigned int)ush_bmpview_row_buf[pix_off + 0ULL];
                g = (unsigned int)ush_bmpview_row_buf[pix_off + 1ULL];
                r = (unsigned int)ush_bmpview_row_buf[pix_off + 2ULL];
                ush_bmpview_render[oy][ox] = ush_bmpview_luma_char(r, g, b);
            }
        }
    }

    (void)cleonos_sys_fd_close(fd);

    {
        u64 oy;
        for (oy = 0ULL; oy < out_h; oy++) {
            ush_writeln(ush_bmpview_render[oy]);
        }
    }

    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "bmpview") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_bmpview(&sh, arg);

    if (has_context != 0) {
        if (ush_streq(sh.cwd, initial_cwd) == 0) {
            ret.flags |= USH_CMD_RET_FLAG_CWD;
            ush_copy(ret.cwd, (u64)sizeof(ret.cwd), sh.cwd);
        }

        if (sh.exit_requested != 0) {
            ret.flags |= USH_CMD_RET_FLAG_EXIT;
            ret.exit_code = sh.exit_code;
        }

        (void)ush_command_ret_write(&ret);
    }

    return (success != 0) ? 0 : 1;
}
