#include "cmd_runtime.h"
#include <png.h>

#define IMGVIEW_MAX_FILE_BYTES (4ULL * 1024ULL * 1024ULL)
#define IMGVIEW_MAX_PIXELS (4096ULL * 4096ULL)
#define IMGVIEW_ASCII_MAX_COLS 160ULL
#define IMGVIEW_ASCII_MAX_ROWS 80ULL

typedef unsigned int imgview_pixel;

typedef struct imgview_mem_reader {
    const unsigned char *data;
    size_t size;
    size_t offset;
    int failed;
} imgview_mem_reader;

typedef struct imgview_image {
    u64 width;
    u64 height;
    imgview_pixel *pixels;
} imgview_image;

typedef struct imgview_args {
    char path[USH_PATH_MAX];
    int ascii;
} imgview_args;

static char imgview_ascii_rows[IMGVIEW_ASCII_MAX_ROWS][IMGVIEW_ASCII_MAX_COLS + 1ULL];

static void imgview_read_cb(png_structp png_ptr, png_bytep out, size_t length) {
    imgview_mem_reader *reader = (imgview_mem_reader *)png_get_io_ptr(png_ptr);

    if (reader == (imgview_mem_reader *)0 || out == (png_bytep)0 || reader->offset > reader->size ||
        length > reader->size - reader->offset) {
        if (reader != (imgview_mem_reader *)0) {
            reader->failed = 1;
        }
        png_error(png_ptr, "imgview read overflow");
        return;
    }

    memcpy(out, reader->data + reader->offset, length);
    reader->offset += length;
}

static void imgview_warn_cb(png_structp png_ptr, png_const_charp message) {
    (void)png_ptr;
    if (message != (png_const_charp)0 && message[0] != '\0') {
        ush_write("imgview: libpng warning: ");
        ush_writeln(message);
    }
}

static int imgview_read_all(const char *path, unsigned char **out_data, u64 *out_size) {
    u64 size;
    u64 fd;
    unsigned char *data;
    u64 done = 0ULL;

    if (path == (const char *)0 || out_data == (unsigned char **)0 || out_size == (u64 *)0) {
        return 0;
    }

    *out_data = (unsigned char *)0;
    *out_size = 0ULL;

    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1 || size == 0ULL || size > IMGVIEW_MAX_FILE_BYTES) {
        return 0;
    }

    data = (unsigned char *)malloc((size_t)size);
    if (data == (unsigned char *)0) {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        free(data);
        return 0;
    }

    while (done < size) {
        u64 got = cleonos_sys_fd_read(fd, data + done, size - done);
        if (got == (u64)-1 || got == 0ULL) {
            (void)cleonos_sys_fd_close(fd);
            free(data);
            return 0;
        }
        done += got;
    }

    (void)cleonos_sys_fd_close(fd);
    *out_data = data;
    *out_size = size;
    return 1;
}

static int imgview_decode_png(const unsigned char *data, u64 data_size, imgview_image *out_image) {
    imgview_mem_reader reader;
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
    png_bytep *rows;
    unsigned char *rgba;
    imgview_pixel *pixels;
    u64 count;
    u64 i;

    if (data == (const unsigned char *)0 || data_size == 0ULL || out_image == (imgview_image *)0) {
        return 0;
    }

    out_image->width = 0ULL;
    out_image->height = 0ULL;
    out_image->pixels = (imgview_pixel *)0;

    ush_zero(&reader, (u64)sizeof(reader));
    reader.data = data;
    reader.size = (size_t)data_size;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, imgview_warn_cb);
    if (png_ptr == (png_structp)0) {
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == (png_infop)0) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 0;
    }

    png_set_read_fn(png_ptr, &reader, imgview_read_cb);
    png_read_info(png_ptr, info_ptr);

    if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                     &interlace_type, &compression_type, &filter_type) == 0) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    if (width == 0U || height == 0U || (u64)width > 4096ULL || (u64)height > 4096ULL ||
        ((u64)width * (u64)height) > IMGVIEW_MAX_PIXELS) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0U) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
        png_set_filler(png_ptr, 0xFFU, PNG_FILLER_AFTER);
    }
    if (interlace_type != PNG_INTERLACE_NONE) {
        (void)png_set_interlace_handling(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes != ((size_t)width * 4U)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    count = (u64)width * (u64)height;
    rgba = (unsigned char *)malloc((size_t)(count * 4ULL));
    rows = (png_bytep *)malloc((size_t)((u64)height * (u64)sizeof(png_bytep)));
    pixels = (imgview_pixel *)malloc((size_t)(count * (u64)sizeof(imgview_pixel)));
    if (rgba == (unsigned char *)0 || rows == (png_bytep *)0 || pixels == (imgview_pixel *)0) {
        if (rgba != (unsigned char *)0) {
            free(rgba);
        }
        if (rows != (png_bytep *)0) {
            free(rows);
        }
        if (pixels != (imgview_pixel *)0) {
            free(pixels);
        }
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    for (i = 0ULL; i < (u64)height; i++) {
        rows[i] = rgba + (i * (u64)rowbytes);
    }

    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    if (reader.failed != 0) {
        free(rgba);
        free(rows);
        free(pixels);
        return 0;
    }

    for (i = 0ULL; i < count; i++) {
        unsigned int r = (unsigned int)rgba[i * 4ULL + 0ULL];
        unsigned int g = (unsigned int)rgba[i * 4ULL + 1ULL];
        unsigned int b = (unsigned int)rgba[i * 4ULL + 2ULL];
        pixels[i] = (imgview_pixel)((r << 16U) | (g << 8U) | b);
    }

    free(rgba);
    free(rows);

    out_image->width = (u64)width;
    out_image->height = (u64)height;
    out_image->pixels = pixels;
    return 1;
}

static imgview_pixel imgview_sample_nearest(const imgview_image *image, u64 x, u64 y, u64 out_w, u64 out_h) {
    u64 src_x;
    u64 src_y;

    if (image == (const imgview_image *)0 || image->pixels == (imgview_pixel *)0 || out_w == 0ULL || out_h == 0ULL) {
        return 0U;
    }

    src_x = (x * image->width) / out_w;
    src_y = (y * image->height) / out_h;
    if (src_x >= image->width) {
        src_x = image->width - 1ULL;
    }
    if (src_y >= image->height) {
        src_y = image->height - 1ULL;
    }

    return image->pixels[src_y * image->width + src_x];
}

static int imgview_compute_fit(u64 src_w, u64 src_h, u64 max_w, u64 max_h, u64 *out_w, u64 *out_h) {
    u64 w = src_w;
    u64 h = src_h;

    if (out_w == (u64 *)0 || out_h == (u64 *)0 || src_w == 0ULL || src_h == 0ULL || max_w == 0ULL || max_h == 0ULL) {
        return 0;
    }

    if (w > max_w) {
        h = (h * max_w) / w;
        w = max_w;
        if (h == 0ULL) {
            h = 1ULL;
        }
    }
    if (h > max_h) {
        w = (w * max_h) / h;
        h = max_h;
        if (w == 0ULL) {
            w = 1ULL;
        }
    }

    *out_w = w;
    *out_h = h;
    return 1;
}

static int imgview_show_framebuffer(const imgview_image *image) {
    cleonos_fb_info fb;
    cleonos_fb_blit_req req;
    imgview_pixel *scaled;
    u64 out_w;
    u64 out_h;
    u64 x;
    u64 y;
    u64 max_w;
    u64 max_h;

    if (image == (const imgview_image *)0 || image->pixels == (imgview_pixel *)0) {
        return 0;
    }

    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return 0;
    }

    max_w = fb.width;
    max_h = fb.height;
    if (max_w > 32ULL) {
        max_w -= 32ULL;
    }
    if (max_h > 64ULL) {
        max_h -= 64ULL;
    }

    if (imgview_compute_fit(image->width, image->height, max_w, max_h, &out_w, &out_h) == 0) {
        return 0;
    }

    scaled = (imgview_pixel *)malloc((size_t)(out_w * out_h * (u64)sizeof(imgview_pixel)));
    if (scaled == (imgview_pixel *)0) {
        return 0;
    }

    for (y = 0ULL; y < out_h; y++) {
        for (x = 0ULL; x < out_w; x++) {
            scaled[y * out_w + x] = imgview_sample_nearest(image, x, y, out_w, out_h);
        }
    }

    req.pixels_ptr = (u64)(usize)scaled;
    req.src_width = out_w;
    req.src_height = out_h;
    req.src_pitch_bytes = out_w * 4ULL;
    req.dst_x = (fb.width > out_w) ? ((fb.width - out_w) / 2ULL) : 0ULL;
    req.dst_y = (fb.height > out_h) ? ((fb.height - out_h) / 2ULL) : 0ULL;
    req.scale = 1ULL;

    if (cleonos_sys_fb_blit(&req) == 0ULL) {
        free(scaled);
        return 0;
    }

    printf("imgview: framebuffer drawn %llux%llu -> %llux%llu\n", (unsigned long long)image->width,
           (unsigned long long)image->height, (unsigned long long)out_w, (unsigned long long)out_h);
    free(scaled);
    return 1;
}

static char imgview_luma_char(imgview_pixel pixel) {
    static const char ramp[] = " .:-=+*#%@";
    unsigned int r = (pixel >> 16U) & 0xFFU;
    unsigned int g = (pixel >> 8U) & 0xFFU;
    unsigned int b = pixel & 0xFFU;
    unsigned int luma = (77U * r + 150U * g + 29U * b) >> 8U;
    unsigned int idx = (luma * 9U) / 255U;
    return ramp[idx];
}

static void imgview_show_ascii(const imgview_image *image) {
    u64 out_w;
    u64 out_h;
    u64 x;
    u64 y;

    if (image == (const imgview_image *)0 || image->pixels == (imgview_pixel *)0) {
        return;
    }

    out_w = image->width;
    out_h = (image->height + 1ULL) / 2ULL;
    if (out_w > IMGVIEW_ASCII_MAX_COLS) {
        out_w = IMGVIEW_ASCII_MAX_COLS;
    }
    if (out_h > IMGVIEW_ASCII_MAX_ROWS) {
        out_h = IMGVIEW_ASCII_MAX_ROWS;
    }
    if (out_w == 0ULL) {
        out_w = 1ULL;
    }
    if (out_h == 0ULL) {
        out_h = 1ULL;
    }

    for (y = 0ULL; y < out_h; y++) {
        for (x = 0ULL; x < out_w; x++) {
            imgview_ascii_rows[y][x] = imgview_luma_char(imgview_sample_nearest(image, x, y, out_w, out_h));
        }
        imgview_ascii_rows[y][out_w] = '\0';
    }

    printf("imgview: ascii preview %llux%llu -> %llux%llu\n", (unsigned long long)image->width,
           (unsigned long long)image->height, (unsigned long long)out_w, (unsigned long long)out_h);
    for (y = 0ULL; y < out_h; y++) {
        ush_writeln(imgview_ascii_rows[y]);
    }
}

static int imgview_parse_arg_line(const char *line, imgview_args *out_args) {
    char first[USH_PATH_MAX];
    char second[32];
    const char *rest = "";
    const char *rest2 = "";

    if (out_args == (imgview_args *)0) {
        return 0;
    }

    out_args->path[0] = '\0';
    out_args->ascii = 0;

    if (line == (const char *)0 || line[0] == '\0') {
        return 0;
    }

    if (ush_split_first_and_rest(line, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0 || ush_streq(first, "help") != 0) {
        return 2;
    }

    if (ush_streq(first, "--ascii") != 0) {
        out_args->ascii = 1;
        if (rest == (const char *)0 || rest[0] == '\0') {
            return 0;
        }
        if (ush_split_first_and_rest(rest, first, (u64)sizeof(first), &rest2) == 0) {
            return 0;
        }
        rest = rest2;
    }

    ush_copy(out_args->path, (u64)sizeof(out_args->path), first);

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }
        if (ush_streq(second, "--ascii") == 0) {
            return 0;
        }
        out_args->ascii = 1;
        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            return 0;
        }
    }

    return 1;
}

static int imgview_parse_argv(int argc, char **argv, imgview_args *out_args) {
    int i;

    if (out_args == (imgview_args *)0) {
        return 0;
    }

    out_args->path[0] = '\0';
    out_args->ascii = 0;

    if (argc <= 1 || argv == (char **)0) {
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == (const char *)0 || arg[0] == '\0') {
            continue;
        }
        if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0 || ush_streq(arg, "help") != 0) {
            return 2;
        }
        if (ush_streq(arg, "--ascii") != 0) {
            out_args->ascii = 1;
            continue;
        }
        if (out_args->path[0] != '\0') {
            return 0;
        }
        ush_copy(out_args->path, (u64)sizeof(out_args->path), arg);
    }

    return (out_args->path[0] != '\0') ? 1 : 0;
}

static void imgview_print_help(void) {
    ush_writeln_i18n("usage: imgview <file.png> [--ascii]", "??: imgview <file.png> [--ascii]");
    ush_writeln_i18n("default: draw PNG to framebuffer; --ascii prints TTY preview",
                     "??: ? PNG ??? framebuffer; --ascii ?? TTY ??");
}

static int imgview_run(const ush_state *sh, const imgview_args *args) {
    char abs_path[USH_PATH_MAX];
    unsigned char *file_data = (unsigned char *)0;
    u64 file_size = 0ULL;
    imgview_image image;
    int ok = 0;

    if (sh == (const ush_state *)0 || args == (const imgview_args *)0 || args->path[0] == '\0') {
        imgview_print_help();
        return 0;
    }

    if (ush_resolve_path(sh, args->path, abs_path, (u64)sizeof(abs_path)) == 0) {
        ush_writeln_i18n("imgview: invalid path", "imgview: ????");
        return 0;
    }

    if (cleonos_sys_fs_stat_type(abs_path) != 1ULL) {
        ush_writeln_i18n("imgview: file not found", "imgview: ?????");
        return 0;
    }

    if (imgview_read_all(abs_path, &file_data, &file_size) == 0) {
        ush_writeln_i18n("imgview: read failed or file too large", "imgview: ?????????");
        return 0;
    }

    ush_zero(&image, (u64)sizeof(image));
    if (imgview_decode_png(file_data, file_size, &image) == 0) {
        free(file_data);
        ush_writeln_i18n("imgview: unsupported or invalid PNG", "imgview: ??????? PNG");
        return 0;
    }
    free(file_data);

    if (args->ascii == 0) {
        ok = imgview_show_framebuffer(&image);
        if (ok == 0) {
            ush_writeln_i18n("imgview: framebuffer unavailable, using ASCII preview",
                             "imgview: framebuffer ?????? ASCII ??");
            imgview_show_ascii(&image);
            ok = 1;
        }
    } else {
        imgview_show_ascii(&image);
        ok = 1;
    }

    free(image.pixels);
    return ok;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    imgview_args args;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int parse_ret;
    int success = 0;

    (void)envp;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(&args, (u64)sizeof(args));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    parse_ret = imgview_parse_argv(argc, argv, &args);

    if (parse_ret == 0 && ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "imgview") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
            parse_ret = imgview_parse_arg_line(ctx.arg, &args);
        }
    }

    if (parse_ret == 2) {
        imgview_print_help();
        success = 1;
    } else if (parse_ret == 1) {
        success = imgview_run(&sh, &args);
    } else {
        imgview_print_help();
        success = 0;
    }

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
