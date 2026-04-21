#include "cmd_runtime.h"
#include "qrcode/qrcodegen.h"

#define USH_QRCODE_MAX_TEXT 640U
#define USH_QRCODE_MAX_VERSION 15
#define USH_QRCODE_BORDER 4U
#define USH_QRCODE_MAX_MODULES ((USH_QRCODE_MAX_VERSION * 4U) + 17U + (USH_QRCODE_BORDER * 2U))
#define USH_QRCODE_CANVAS_MAX 1024U

#define USH_QRCODE_COLOR_DARK 0x00000000ULL
#define USH_QRCODE_COLOR_LIGHT 0x00FFFFFFULL

static unsigned int ush_qrcode_canvas[USH_QRCODE_CANVAS_MAX][USH_QRCODE_CANVAS_MAX];

static int ush_qrcode_streq_ignore_case(const char *left, const char *right) {
    u64 i = 0ULL;

    if (left == (const char *)0 || right == (const char *)0) {
        return 0;
    }

    while (left[i] != '\0' && right[i] != '\0') {
        char lc = left[i];
        char rc = right[i];

        if (lc >= 'a' && lc <= 'z') {
            lc = (char)(lc - ('a' - 'A'));
        }

        if (rc >= 'a' && rc <= 'z') {
            rc = (char)(rc - ('a' - 'A'));
        }

        if (lc != rc) {
            return 0;
        }

        i++;
    }

    return (left[i] == '\0' && right[i] == '\0') ? 1 : 0;
}

static int ush_qrcode_parse_ecc(const char *text, enum qrcodegen_Ecc *out_ecc) {
    if (text == (const char *)0 || out_ecc == (enum qrcodegen_Ecc *)0) {
        return 0;
    }

    if (ush_qrcode_streq_ignore_case(text, "L") != 0 || ush_qrcode_streq_ignore_case(text, "LOW") != 0) {
        *out_ecc = qrcodegen_Ecc_LOW;
        return 1;
    }

    if (ush_qrcode_streq_ignore_case(text, "M") != 0 || ush_qrcode_streq_ignore_case(text, "MEDIUM") != 0) {
        *out_ecc = qrcodegen_Ecc_MEDIUM;
        return 1;
    }

    if (ush_qrcode_streq_ignore_case(text, "Q") != 0 || ush_qrcode_streq_ignore_case(text, "QUARTILE") != 0) {
        *out_ecc = qrcodegen_Ecc_QUARTILE;
        return 1;
    }

    if (ush_qrcode_streq_ignore_case(text, "H") != 0 || ush_qrcode_streq_ignore_case(text, "HIGH") != 0) {
        *out_ecc = qrcodegen_Ecc_HIGH;
        return 1;
    }

    return 0;
}

static void ush_qrcode_usage(void) {
    ush_writeln("usage: qrcode [--ecc <L|M|Q|H>] <text>");
    ush_writeln("       qrcode --help");
    ush_writeln("note: pipeline input supported when <text> omitted");
}

/* return: 0 fail, 1 ok, 2 help */
static int ush_qrcode_parse_args(const char *arg, char *out_text, u64 out_text_size, enum qrcodegen_Ecc *out_ecc) {
    char first[USH_PATH_MAX];
    char second[USH_PATH_MAX];
    const char *rest = "";
    const char *rest2 = "";

    if (out_text == (char *)0 || out_text_size == 0ULL || out_ecc == (enum qrcodegen_Ecc *)0) {
        return 0;
    }

    *out_ecc = qrcodegen_Ecc_LOW;
    out_text[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_text[0] != '\0') {
            if (ush_pipeline_stdin_len + 1ULL > out_text_size) {
                return 0;
            }

            ush_copy(out_text, out_text_size, ush_pipeline_stdin_text);
            return 1;
        }

        return 0;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    if (ush_streq(first, "--help") != 0 || ush_streq(first, "-h") != 0) {
        return 2;
    }

    if (ush_streq(first, "--ecc") != 0) {
        if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
            return 0;
        }

        if (ush_qrcode_parse_ecc(second, out_ecc) == 0) {
            return 0;
        }

        if (rest2 == (const char *)0 || rest2[0] == '\0') {
            if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_text[0] != '\0') {
                if (ush_pipeline_stdin_len + 1ULL > out_text_size) {
                    return 0;
                }

                ush_copy(out_text, out_text_size, ush_pipeline_stdin_text);
                return 1;
            }

            return 0;
        }

        ush_copy(out_text, out_text_size, rest2);
        return 1;
    }

    if (first[0] == '-' && first[1] == '-') {
        const char *prefix = "--ecc=";
        u64 i = 0ULL;
        int match = 1;

        while (prefix[i] != '\0') {
            if (first[i] != prefix[i]) {
                match = 0;
                break;
            }

            i++;
        }

        if (match != 0) {
            if (first[i] == '\0') {
                return 0;
            }

            if (ush_qrcode_parse_ecc(&first[i], out_ecc) == 0) {
                return 0;
            }

            if (rest == (const char *)0 || rest[0] == '\0') {
                if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_text[0] != '\0') {
                    if (ush_pipeline_stdin_len + 1ULL > out_text_size) {
                        return 0;
                    }

                    ush_copy(out_text, out_text_size, ush_pipeline_stdin_text);
                    return 1;
                }

                return 0;
            }

            ush_copy(out_text, out_text_size, rest);
            return 1;
        }
    }

    ush_copy(out_text, out_text_size, arg);
    return 1;
}

static void ush_qrcode_emit_ascii(const uint8_t qrcode[]) {
    int size = qrcodegen_getSize(qrcode);
    int border = (int)USH_QRCODE_BORDER;
    int limit;
    int y;
    int x;

    limit = size + border;

    for (y = -border; y < limit; y++) {
        for (x = -border; x < limit; x++) {
            int dark = (x >= 0 && y >= 0 && x < size && y < size && qrcodegen_getModule(qrcode, x, y)) ? 1 : 0;
            ush_write(dark != 0 ? "##" : "  ");
        }

        ush_write_char('\n');
    }
}

static int ush_qrcode_build_canvas(const uint8_t qrcode[], u64 module_pixels, u64 *out_side_pixels) {
    int qr_size = qrcodegen_getSize(qrcode);
    u64 side_modules;
    u64 side_pixels;
    u64 y;
    u64 x;

    if (out_side_pixels == (u64 *)0 || module_pixels == 0ULL || qr_size <= 0) {
        return 0;
    }

    side_modules = (u64)qr_size + (u64)(USH_QRCODE_BORDER * 2U);
    if (side_modules == 0ULL || side_modules > (u64)USH_QRCODE_MAX_MODULES) {
        return 0;
    }

    side_pixels = side_modules * module_pixels;
    if (side_pixels == 0ULL || side_pixels > (u64)USH_QRCODE_CANVAS_MAX) {
        return 0;
    }

    for (y = 0ULL; y < side_pixels; y++) {
        for (x = 0ULL; x < side_pixels; x++) {
            int qx = (int)(x / module_pixels) - (int)USH_QRCODE_BORDER;
            int qy = (int)(y / module_pixels) - (int)USH_QRCODE_BORDER;
            int dark =
                (qx >= 0 && qy >= 0 && qx < qr_size && qy < qr_size && qrcodegen_getModule(qrcode, qx, qy)) ? 1 : 0;
            ush_qrcode_canvas[y][x] =
                (dark != 0) ? (unsigned int)USH_QRCODE_COLOR_DARK : (unsigned int)USH_QRCODE_COLOR_LIGHT;
        }
    }

    *out_side_pixels = side_pixels;
    return 1;
}

static int ush_qrcode_emit_pixels(const uint8_t qrcode[]) {
    int qr_size = qrcodegen_getSize(qrcode);
    u64 side_modules;
    cleonos_fb_info fb_info;
    cleonos_fb_blit_req req;
    u64 module_pixels_x;
    u64 module_pixels_y;
    u64 module_pixels;
    u64 module_pixels_canvas_cap;
    u64 side_pixels = 0ULL;
    u64 draw_w;
    u64 draw_h;

    if (qr_size <= 0) {
        return 0;
    }

    side_modules = (u64)qr_size + (u64)(USH_QRCODE_BORDER * 2U);
    if (side_modules == 0ULL || side_modules > (u64)USH_QRCODE_MAX_MODULES) {
        return 0;
    }

    ush_zero(&fb_info, (u64)sizeof(fb_info));
    if (cleonos_sys_fb_info(&fb_info) == 0ULL || fb_info.width == 0ULL || fb_info.height == 0ULL ||
        fb_info.bpp != 32ULL) {
        ush_writeln("qrcode: framebuffer unavailable, fallback to ascii");
        ush_qrcode_emit_ascii(qrcode);
        return 1;
    }

    module_pixels_x = fb_info.width / side_modules;
    module_pixels_y = fb_info.height / side_modules;
    module_pixels = (module_pixels_x < module_pixels_y) ? module_pixels_x : module_pixels_y;

    if (module_pixels == 0ULL) {
        ush_writeln("qrcode: framebuffer too small");
        return 0;
    }

    module_pixels_canvas_cap = (u64)USH_QRCODE_CANVAS_MAX / side_modules;
    if (module_pixels_canvas_cap == 0ULL) {
        return 0;
    }

    if (module_pixels > module_pixels_canvas_cap) {
        module_pixels = module_pixels_canvas_cap;
    }

    if (ush_qrcode_build_canvas(qrcode, module_pixels, &side_pixels) == 0) {
        return 0;
    }

    draw_w = side_pixels;
    draw_h = side_pixels;

    req.pixels_ptr = (u64)(void *)&ush_qrcode_canvas[0][0];
    req.src_width = side_pixels;
    req.src_height = side_pixels;
    req.src_pitch_bytes = (u64)USH_QRCODE_CANVAS_MAX * 4ULL;
    req.dst_x = (fb_info.width > draw_w) ? ((fb_info.width - draw_w) / 2ULL) : 0ULL;
    req.dst_y = (fb_info.height > draw_h) ? ((fb_info.height - draw_h) / 2ULL) : 0ULL;
    req.scale = 1ULL;

    (void)cleonos_sys_fb_clear(USH_QRCODE_COLOR_LIGHT);
    if (cleonos_sys_fb_blit(&req) == 0ULL) {
        ush_writeln("qrcode: framebuffer blit failed");
        return 0;
    }

    return 1;
}

static int ush_cmd_qrcode(const char *arg) {
    char text[USH_QRCODE_MAX_TEXT];
    enum qrcodegen_Ecc ecc;
    uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(USH_QRCODE_MAX_VERSION)];
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(USH_QRCODE_MAX_VERSION)];
    int parse_ret;
    int ok;

    parse_ret = ush_qrcode_parse_args(arg, text, (u64)sizeof(text), &ecc);
    if (parse_ret == 2) {
        ush_qrcode_usage();
        return 1;
    }

    if (parse_ret == 0 || text[0] == '\0') {
        ush_qrcode_usage();
        return 0;
    }

    ok = qrcodegen_encodeText(text, temp, qrcode, ecc, qrcodegen_VERSION_MIN, USH_QRCODE_MAX_VERSION,
                              qrcodegen_Mask_AUTO, true);

    if (ok == 0) {
        ush_writeln("qrcode: encode failed (input too long or invalid)");
        return 0;
    }

    return ush_qrcode_emit_pixels(qrcode);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "qrcode") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_qrcode(arg);

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
