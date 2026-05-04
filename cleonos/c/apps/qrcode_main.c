#include "cmd_runtime.h"
#include "qrcode/qrcodegen.h"

#define USH_QRCODE_MAX_TEXT 640U
#define USH_QRCODE_MAX_VERSION 15
#define USH_QRCODE_BORDER 4U
#define USH_QRCODE_MAX_MODULES ((USH_QRCODE_MAX_VERSION * 4U) + 17U + (USH_QRCODE_BORDER * 2U))
#define USH_QRCODE_CANVAS_MAX 640U
#define USH_QRCODE_TTY_DISPLAY 1ULL
#define USH_QRCODE_TITLE_H 34
#define USH_QRCODE_FOOTER_H 34
#define USH_QRCODE_PAD 24
#define USH_QRCODE_CLOSE_W 44
#define USH_QRCODE_WINDOW_DEFAULT_W 520
#define USH_QRCODE_WINDOW_DEFAULT_H 600
#define USH_QRCODE_WINDOW_MIN_W 320
#define USH_QRCODE_WINDOW_MIN_H 380

#define USH_QRCODE_COLOR_DARK 0x00000000U
#define USH_QRCODE_COLOR_LIGHT 0x00FFFFFFU
#define USH_QRCODE_COLOR_DESKTOP 0x00F3F3F3U
#define USH_QRCODE_COLOR_TITLE 0x000078D7U
#define USH_QRCODE_COLOR_CLOSE 0x00E81123U
#define USH_QRCODE_COLOR_TEXT 0x00232323U
#define USH_QRCODE_COLOR_MUTED 0x00666666U
#define USH_QRCODE_COLOR_BORDER 0x00D0D0D0U
#define USH_QRCODE_COLOR_PANEL 0x00FFFFFFU

#define USH_QRCODE_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                  \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

static unsigned int ush_qrcode_canvas[USH_QRCODE_CANVAS_MAX][USH_QRCODE_CANVAS_MAX];

static int ush_qrcode_clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

static int ush_qrcode_u64_as_i32(u64 raw) {
    return (int)(i64)raw;
}

static char ush_qrcode_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }

    return ch;
}

static u64 ush_qrcode_glyph_mask(char ch) {
    switch (ush_qrcode_upper_char(ch)) {
    case 'C':
        return USH_QRCODE_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return USH_QRCODE_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return USH_QRCODE_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'L':
        return USH_QRCODE_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'O':
        return USH_QRCODE_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return USH_QRCODE_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return USH_QRCODE_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return USH_QRCODE_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return USH_QRCODE_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return USH_QRCODE_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    default:
        return 0ULL;
    }
}

static void ush_qrcode_fill_rect(int canvas_w, int canvas_h, int x, int y, int w, int h, unsigned int color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (canvas_w <= 0 || canvas_h <= 0 || canvas_w > (int)USH_QRCODE_CANVAS_MAX ||
        canvas_h > (int)USH_QRCODE_CANVAS_MAX || w <= 0 || h <= 0) {
        return;
    }

    left = ush_qrcode_clampi(x, 0, canvas_w);
    top = ush_qrcode_clampi(y, 0, canvas_h);
    right = ush_qrcode_clampi(x + w, 0, canvas_w);
    bottom = ush_qrcode_clampi(y + h, 0, canvas_h);
    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        int col;
        for (col = left; col < right; col++) {
            ush_qrcode_canvas[row][col] = color;
        }
    }
}

static void ush_qrcode_stroke_rect(int canvas_w, int canvas_h, int x, int y, int w, int h, unsigned int color) {
    ush_qrcode_fill_rect(canvas_w, canvas_h, x, y, w, 1, color);
    ush_qrcode_fill_rect(canvas_w, canvas_h, x, y + h - 1, w, 1, color);
    ush_qrcode_fill_rect(canvas_w, canvas_h, x, y, 1, h, color);
    ush_qrcode_fill_rect(canvas_w, canvas_h, x + w - 1, y, 1, h, color);
}

static void ush_qrcode_draw_char(int canvas_w, int canvas_h, int x, int y, char ch, int scale, unsigned int color) {
    u64 mask = ush_qrcode_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }

    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit_index = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit_index)) != 0ULL) {
                ush_qrcode_fill_rect(canvas_w, canvas_h, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void ush_qrcode_draw_text(int canvas_w, int canvas_h, int x, int y, const char *text, int scale,
                                 unsigned int color) {
    int cursor_x = x;

    if (text == (const char *)0 || scale <= 0) {
        return;
    }

    while (*text != '\0' && cursor_x + (5 * scale) < canvas_w) {
        if (*text != ' ') {
            ush_qrcode_draw_char(canvas_w, canvas_h, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

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
    ush_writeln_i18n("usage: qrcode [--ecc <L|M|Q|H>] <text>",
                     "用法: qrcode [--ecc <L|M|Q|H>] <text>");
    ush_writeln("       qrcode --help");
    ush_writeln_i18n("note: pipeline input supported when <text> omitted",
                     "提示: 省略 <text> 时支持管道输入");
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

static void ush_qrcode_choose_window_geometry(const cleonos_fb_info *fb_info, int *out_x, int *out_y, int *out_w,
                                              int *out_h) {
    int screen_w = 800;
    int screen_h = 600;
    int max_w;
    int max_h;
    int win_w;
    int win_h;

    if (fb_info != (const cleonos_fb_info *)0 && fb_info->width > 0ULL && fb_info->height > 0ULL &&
        fb_info->width <= 4096ULL && fb_info->height <= 4096ULL) {
        screen_w = (int)fb_info->width;
        screen_h = (int)fb_info->height;
    }

    max_w = screen_w - 80;
    max_h = screen_h - 96;
    if (max_w < USH_QRCODE_WINDOW_MIN_W) {
        max_w = screen_w;
    }
    if (max_h < USH_QRCODE_WINDOW_MIN_H) {
        max_h = screen_h;
    }

    win_w = ush_qrcode_clampi(USH_QRCODE_WINDOW_DEFAULT_W, USH_QRCODE_WINDOW_MIN_W, max_w);
    win_h = ush_qrcode_clampi(USH_QRCODE_WINDOW_DEFAULT_H, USH_QRCODE_WINDOW_MIN_H, max_h);
    if (win_w > (int)USH_QRCODE_CANVAS_MAX) {
        win_w = (int)USH_QRCODE_CANVAS_MAX;
    }
    if (win_h > (int)USH_QRCODE_CANVAS_MAX) {
        win_h = (int)USH_QRCODE_CANVAS_MAX;
    }

    *out_w = win_w;
    *out_h = win_h;
    *out_x = (screen_w > win_w) ? ((screen_w - win_w) / 2) : 0;
    *out_y = (screen_h > win_h) ? ((screen_h - win_h) / 2) : 0;
}

static int ush_qrcode_draw_window_canvas(const uint8_t qrcode[], int canvas_w, int canvas_h) {
    int qr_size = qrcodegen_getSize(qrcode);
    int border = (int)USH_QRCODE_BORDER;
    int side_modules;
    int content_w;
    int content_h;
    int qr_area;
    int module_pixels;
    int side_pixels;
    int qr_x;
    int qr_y;
    int y;

    if (qrcode == (const uint8_t *)0 || qr_size <= 0 || canvas_w <= 0 || canvas_h <= 0 ||
        canvas_w > (int)USH_QRCODE_CANVAS_MAX || canvas_h > (int)USH_QRCODE_CANVAS_MAX) {
        return 0;
    }

    side_modules = qr_size + (border * 2);
    if (side_modules <= 0 || side_modules > (int)USH_QRCODE_MAX_MODULES) {
        return 0;
    }

    content_w = canvas_w - (USH_QRCODE_PAD * 2);
    content_h = canvas_h - USH_QRCODE_TITLE_H - USH_QRCODE_FOOTER_H - (USH_QRCODE_PAD * 2);
    qr_area = (content_w < content_h) ? content_w : content_h;
    module_pixels = qr_area / side_modules;
    if (module_pixels <= 0) {
        return 0;
    }

    side_pixels = side_modules * module_pixels;
    qr_x = (canvas_w - side_pixels) / 2;
    qr_y = USH_QRCODE_TITLE_H + USH_QRCODE_PAD + ((content_h - side_pixels) / 2);

    ush_qrcode_fill_rect(canvas_w, canvas_h, 0, 0, canvas_w, canvas_h, USH_QRCODE_COLOR_DESKTOP);
    ush_qrcode_fill_rect(canvas_w, canvas_h, 0, 0, canvas_w, USH_QRCODE_TITLE_H, USH_QRCODE_COLOR_TITLE);
    ush_qrcode_fill_rect(canvas_w, canvas_h, canvas_w - USH_QRCODE_CLOSE_W, 0, USH_QRCODE_CLOSE_W, USH_QRCODE_TITLE_H,
                         USH_QRCODE_COLOR_CLOSE);
    ush_qrcode_draw_text(canvas_w, canvas_h, 14, 10, "QRCODE", 1, USH_QRCODE_COLOR_LIGHT);
    ush_qrcode_fill_rect(canvas_w, canvas_h, canvas_w - 28, 11, 14, 2, USH_QRCODE_COLOR_LIGHT);
    ush_qrcode_fill_rect(canvas_w, canvas_h, canvas_w - 28, 22, 14, 2, USH_QRCODE_COLOR_LIGHT);
    ush_qrcode_fill_rect(canvas_w, canvas_h, canvas_w - 22, 15, 2, 6, USH_QRCODE_COLOR_LIGHT);

    ush_qrcode_fill_rect(canvas_w, canvas_h, qr_x - 10, qr_y - 10, side_pixels + 20, side_pixels + 20,
                         USH_QRCODE_COLOR_PANEL);
    ush_qrcode_stroke_rect(canvas_w, canvas_h, qr_x - 10, qr_y - 10, side_pixels + 20, side_pixels + 20,
                           USH_QRCODE_COLOR_BORDER);
    ush_qrcode_fill_rect(canvas_w, canvas_h, qr_x, qr_y, side_pixels, side_pixels, USH_QRCODE_COLOR_LIGHT);

    for (y = -border; y < qr_size + border; y++) {
        int x;
        for (x = -border; x < qr_size + border; x++) {
            int dark = (x >= 0 && y >= 0 && x < qr_size && y < qr_size && qrcodegen_getModule(qrcode, x, y)) ? 1 : 0;
            if (dark != 0) {
                int px = qr_x + ((x + border) * module_pixels);
                int py = qr_y + ((y + border) * module_pixels);
                ush_qrcode_fill_rect(canvas_w, canvas_h, px, py, module_pixels, module_pixels, USH_QRCODE_COLOR_DARK);
            }
        }
    }

    ush_qrcode_draw_text(canvas_w, canvas_h, 22, canvas_h - 24, "PRESS Q TO CLOSE", 1, USH_QRCODE_COLOR_MUTED);
    return 1;
}

static int ush_qrcode_present_window(u64 window_id, int width, int height) {
    cleonos_wm_present_req req;

    if (window_id == 0ULL || width <= 0 || height <= 0) {
        return 0;
    }

    req.window_id = window_id;
    req.pixels_ptr = (u64)(usize)&ush_qrcode_canvas[0][0];
    req.src_width = (u64)(unsigned int)width;
    req.src_height = (u64)(unsigned int)height;
    req.src_pitch_bytes = (u64)USH_QRCODE_CANVAS_MAX * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

static int ush_qrcode_close_hit(int width, int local_x, int local_y) {
    return (local_x >= width - USH_QRCODE_CLOSE_W && local_x < width && local_y >= 0 && local_y < USH_QRCODE_TITLE_H)
               ? 1
               : 0;
}

static void ush_qrcode_window_loop(u64 window_id, int width, int height, int x, int y) {
    int running = 1;
    int dragging = 0;
    int drag_offset_x = 0;
    int drag_offset_y = 0;

    (void)height;

    while (running != 0) {
        u64 budget = 0ULL;
        int handled = 0;

        while (budget < 64ULL) {
            cleonos_wm_event event;
            ush_zero(&event, (u64)sizeof(event));
            if (cleonos_sys_wm_poll_event(window_id, &event) == 0ULL) {
                break;
            }

            handled = 1;
            if (event.type == CLEONOS_WM_EVENT_KEY) {
                if (event.arg0 == (u64)'q' || event.arg0 == (u64)'Q' || event.arg0 == 27ULL || event.arg0 == 13ULL) {
                    running = 0;
                    break;
                }
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_BUTTON) {
                u64 buttons = event.arg0;
                u64 changed = event.arg1;
                int local_x = ush_qrcode_u64_as_i32(event.arg2);
                int local_y = ush_qrcode_u64_as_i32(event.arg3);
                int left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
                int left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

                if (left_changed != 0) {
                    if (left_down == 0) {
                        dragging = 0;
                    } else if (ush_qrcode_close_hit(width, local_x, local_y) != 0) {
                        running = 0;
                        break;
                    } else if (local_y >= 0 && local_y < USH_QRCODE_TITLE_H) {
                        dragging = 1;
                        drag_offset_x = local_x;
                        drag_offset_y = local_y;
                    }
                }
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_MOVE && dragging != 0) {
                cleonos_wm_move_req move_req;
                x = ush_qrcode_u64_as_i32(event.arg0) - drag_offset_x;
                y = ush_qrcode_u64_as_i32(event.arg1) - drag_offset_y;
                move_req.window_id = window_id;
                move_req.x = (u64)(i64)x;
                move_req.y = (u64)(i64)y;
                (void)cleonos_sys_wm_move(&move_req);
            }

            budget++;
        }

        if (running == 0) {
            break;
        }

        if (handled != 0) {
            (void)cleonos_sys_yield();
        } else {
            (void)cleonos_sys_sleep_ticks(1ULL);
        }
    }
}

static int ush_qrcode_emit_window(const uint8_t qrcode[]) {
    int qr_size = qrcodegen_getSize(qrcode);
    cleonos_fb_info fb_info;
    cleonos_wm_create_req create_req;
    u64 old_tty;
    u64 window_id;
    int win_x;
    int win_y;
    int win_w;
    int win_h;

    if (qr_size <= 0) {
        return 0;
    }

    ush_zero(&fb_info, (u64)sizeof(fb_info));
    if (cleonos_sys_fb_info(&fb_info) == 0ULL || fb_info.width == 0ULL || fb_info.height == 0ULL ||
        fb_info.bpp != 32ULL) {
        ush_writeln_i18n("qrcode: desktop unavailable, fallback to ascii",
                         "qrcode: 桌面不可用，回退到 ASCII 输出");
        ush_qrcode_emit_ascii(qrcode);
        return 1;
    }

    ush_qrcode_choose_window_geometry(&fb_info, &win_x, &win_y, &win_w, &win_h);
    if (ush_qrcode_draw_window_canvas(qrcode, win_w, win_h) == 0) {
        ush_writeln_i18n("qrcode: desktop window too small", "qrcode: 桌面窗口过小");
        return 0;
    }

    old_tty = cleonos_sys_tty_active();
    if (old_tty != USH_QRCODE_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(USH_QRCODE_TTY_DISPLAY);
    }

    create_req.x = (u64)(i64)win_x;
    create_req.y = (u64)(i64)win_y;
    create_req.width = (u64)(unsigned int)win_w;
    create_req.height = (u64)(unsigned int)win_h;
    create_req.flags = CLEONOS_WM_FLAG_TOPMOST;
    window_id = cleonos_sys_wm_create(&create_req);
    if (window_id == 0ULL) {
        if (old_tty != USH_QRCODE_TTY_DISPLAY) {
            (void)cleonos_sys_tty_switch(old_tty);
        }
        ush_writeln_i18n("qrcode: wm window create failed, fallback to ascii",
                         "qrcode: WM 窗口创建失败，回退到 ASCII 输出");
        ush_qrcode_emit_ascii(qrcode);
        return 1;
    }

    if (ush_qrcode_present_window(window_id, win_w, win_h) == 0) {
        (void)cleonos_sys_wm_destroy(window_id);
        if (old_tty != USH_QRCODE_TTY_DISPLAY) {
            (void)cleonos_sys_tty_switch(old_tty);
        }
        ush_writeln_i18n("qrcode: wm present failed", "qrcode: WM 显示失败");
        return 0;
    }

    (void)cleonos_sys_wm_set_focus(window_id);
    ush_qrcode_window_loop(window_id, win_w, win_h, win_x, win_y);
    (void)cleonos_sys_wm_destroy(window_id);
    if (old_tty != USH_QRCODE_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(old_tty);
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
        ush_writeln_i18n("qrcode: encode failed (input too long or invalid)",
                         "qrcode: 编码失败（输入过长或无效）");
        return 0;
    }

    return ush_qrcode_emit_window(qrcode);
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
