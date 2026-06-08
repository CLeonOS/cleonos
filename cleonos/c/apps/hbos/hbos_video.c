#include "hbos.h"
#include <stdlib.h>

static const hbos_u8 hbos_font8x8_basic[96][8] = {
    {0,0,0,0,0,0,0,0},{24,60,60,24,24,0,24,0},{54,54,20,0,0,0,0,0},{54,54,127,54,127,54,54,0},
    {24,62,3,30,48,31,24,0},{0,99,51,24,12,102,99,0},{28,54,28,59,102,102,59,0},{12,12,6,0,0,0,0,0},
    {24,12,6,6,6,12,24,0},{6,12,24,24,24,12,6,0},{0,102,60,255,60,102,0,0},{0,24,24,126,24,24,0,0},
    {0,0,0,0,0,24,24,12},{0,0,0,126,0,0,0,0},{0,0,0,0,0,24,24,0},{96,48,24,12,6,3,1,0},
    {62,99,115,123,111,103,62,0},{24,28,24,24,24,24,126,0},{62,99,96,48,24,12,127,0},{62,99,96,56,96,99,62,0},
    {48,56,60,54,127,48,120,0},{127,3,63,96,96,99,62,0},{60,6,3,63,99,99,62,0},{127,99,48,24,12,12,12,0},
    {62,99,99,62,99,99,62,0},{62,99,99,126,96,48,30,0},{0,24,24,0,0,24,24,0},{0,24,24,0,0,24,24,12},
    {48,24,12,6,12,24,48,0},{0,0,126,0,126,0,0,0},{6,12,24,48,24,12,6,0},{62,99,48,24,24,0,24,0},
    {62,99,123,123,123,3,62,0},{24,60,102,102,126,102,102,0},{63,102,102,62,102,102,63,0},{60,102,3,3,3,102,60,0},
    {31,54,102,102,102,54,31,0},{127,70,22,30,22,70,127,0},{127,70,22,30,22,6,15,0},{60,102,3,3,115,102,124,0},
    {102,102,102,126,102,102,102,0},{60,24,24,24,24,24,60,0},{120,48,48,48,51,51,30,0},{103,102,54,30,54,102,103,0},
    {15,6,6,6,70,102,127,0},{99,119,127,107,99,99,99,0},{99,103,111,123,115,99,99,0},{62,99,99,99,99,99,62,0},
    {63,102,102,62,6,6,15,0},{62,99,99,99,107,51,94,0},{63,102,102,62,54,102,103,0},{62,99,7,62,112,99,62,0},
    {126,90,24,24,24,24,60,0},{102,102,102,102,102,102,62,0},{102,102,102,102,102,60,24,0},{99,99,99,107,127,119,99,0},
    {99,99,54,28,54,99,99,0},{102,102,102,60,24,24,60,0},{127,99,49,24,76,102,127,0},{30,6,6,6,6,6,30,0},
    {3,6,12,24,48,96,64,0},{30,24,24,24,24,24,30,0},{8,28,54,99,0,0,0,0},{0,0,0,0,0,0,0,255},
    {24,24,48,0,0,0,0,0},{0,0,62,96,126,99,126,0},{7,6,6,62,102,102,59,0},{0,0,62,99,3,99,62,0},
    {56,48,48,124,54,54,124,0},{0,0,62,99,127,3,62,0},{28,54,6,15,6,6,15,0},{0,0,124,54,54,124,48,31},
    {7,6,54,110,102,102,103,0},{24,0,28,24,24,24,60,0},{48,0,56,48,48,51,51,30},{7,6,102,54,30,54,103,0},
    {28,24,24,24,24,24,60,0},{0,0,55,127,107,99,99,0},{0,0,59,102,102,102,102,0},{0,0,62,99,99,99,62,0},
    {0,0,59,102,102,62,6,15},{0,0,124,54,54,124,48,120},{0,0,59,110,6,6,15,0},{0,0,126,3,62,96,63,0},
    {8,12,63,12,12,108,56,0},{0,0,102,102,102,102,126,0},{0,0,102,102,102,60,24,0},{0,0,99,99,107,127,54,0},
    {0,0,99,54,28,54,99,0},{0,0,102,102,102,126,96,63},{0,0,127,49,24,70,127,0},{56,12,12,7,12,12,56,0},
    {24,24,24,0,24,24,24,0},{7,12,12,56,12,12,7,0},{110,59,0,0,0,0,0,0},{0,24,60,102,66,66,126,0}
};

static hbos_u8 hbos_glyph_row(char ch, int row) {
    unsigned char c = (unsigned char)ch;
    if (c < 32U || c > 127U || row < 0 || row >= 8) {
        return 0U;
    }
    return hbos_font8x8_basic[c - 32U][row];
}

void hbos_video_clear(hbos_state *state, hbos_u32 color) {
    hbos_u32 i;
    if (state == (hbos_state *)0 || state->pixels == (hbos_u32 *)0) {
        return;
    }
    for (i = 0U; i < HBOS_SCREEN_W * HBOS_SCREEN_H; i++) {
        state->pixels[i] = color;
    }
}

void hbos_video_rect(hbos_state *state, int x, int y, int w, int h, hbos_u32 color) {
    int yy;
    int xx;
    if (state == (hbos_state *)0 || state->pixels == (hbos_u32 *)0 || w <= 0 || h <= 0) {
        return;
    }
    for (yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < 0 || py >= (int)HBOS_SCREEN_H) {
            continue;
        }
        for (xx = 0; xx < w; xx++) {
            int px = x + xx;
            if (px >= 0 && px < (int)HBOS_SCREEN_W) {
                state->pixels[(hbos_u32)py * HBOS_SCREEN_W + (hbos_u32)px] = color;
            }
        }
    }
}

void hbos_video_frame(hbos_state *state, int x, int y, int w, int h, hbos_u32 light, hbos_u32 dark) {
    hbos_video_rect(state, x, y, w, 1, light);
    hbos_video_rect(state, x, y, 1, h, light);
    hbos_video_rect(state, x, y + h - 1, w, 1, dark);
    hbos_video_rect(state, x + w - 1, y, 1, h, dark);
}

static void hbos_video_char(hbos_state *state, int x, int y, char ch, hbos_u32 fg, hbos_u32 bg, int scale) {
    int row;
    int col;
    if (scale < 1) {
        scale = 1;
    }
    if (bg != 0xFFFFFFFFU) {
        hbos_video_rect(state, x, y, 8 * scale, 8 * scale, bg);
    }
    for (row = 0; row < 8; row++) {
        hbos_u8 bits = hbos_glyph_row(ch, row);
        for (col = 0; col < 8; col++) {
            if ((bits & (hbos_u8)(0x80U >> (hbos_u32)col)) != 0U) {
                hbos_video_rect(state, x + col * scale, y + row * scale, scale, scale, fg);
            }
        }
    }
}

void hbos_video_text_limit(hbos_state *state, int x, int y, const char *text, hbos_u32 fg, hbos_u32 bg, int scale,
                           int max_chars) {
    int i = 0;
    if (text == (const char *)0) {
        return;
    }
    while (text[i] != '\0' && (max_chars < 0 || i < max_chars)) {
        hbos_video_char(state, x + i * 8 * scale, y, text[i], fg, bg, scale);
        i++;
    }
}

void hbos_video_text(hbos_state *state, int x, int y, const char *text, hbos_u32 fg, hbos_u32 bg, int scale) {
    hbos_video_text_limit(state, x, y, text, fg, bg, scale, -1);
}

static void hbos_draw_taskbar(hbos_state *state) {
    hbos_video_rect(state, 0, 372, 640, 28, HBOS_COLOR_PANEL);
    hbos_video_frame(state, 0, 372, 640, 28, HBOS_COLOR_PANEL_LIGHT, HBOS_COLOR_PANEL_DARK);
    hbos_video_rect(state, 8, 378, 72, 18, 0x00E0E0E0U);
    hbos_video_frame(state, 8, 378, 72, 18, HBOS_COLOR_PANEL_LIGHT, HBOS_COLOR_PANEL_DARK);
    hbos_video_text(state, 18, 383, "Start", HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
    hbos_video_rect(state, 92, 378, 146, 18, 0x00D8D8D8U);
    hbos_video_frame(state, 92, 378, 146, 18, HBOS_COLOR_PANEL_DARK, HBOS_COLOR_PANEL_LIGHT);
    hbos_video_text(state, 102, 383, "haribote console", HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
    hbos_video_text(state, 520, 383, "CLeonOS host", HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
}

static void hbos_draw_window(hbos_state *state, int x, int y, int w, int h, const char *title) {
    hbos_video_rect(state, x, y, w, h, HBOS_COLOR_PANEL);
    hbos_video_frame(state, x, y, w, h, HBOS_COLOR_PANEL_LIGHT, HBOS_COLOR_PANEL_DARK);
    hbos_video_rect(state, x + 3, y + 3, w - 6, 18, HBOS_COLOR_TITLE);
    hbos_video_text_limit(state, x + 8, y + 8, title, HBOS_COLOR_TITLE_TEXT, 0xFFFFFFFFU, 1, (w - 18) / 8);
    hbos_video_rect(state, x + w - 20, y + 6, 12, 12, HBOS_COLOR_PANEL);
    hbos_video_frame(state, x + w - 20, y + 6, 12, 12, HBOS_COLOR_PANEL_LIGHT, HBOS_COLOR_PANEL_DARK);
    hbos_video_text(state, x + w - 17, y + 8, "x", HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
}

static void hbos_draw_console(hbos_state *state) {
    hbos_u32 i;
    hbos_u32 start = 0U;
    int line_y;

    hbos_draw_window(state, 16, 126, 608, 238, "HariboteOS terminal - hbos user emulator");
    hbos_video_rect(state, 22, 150, 596, 205, HBOS_COLOR_CONSOLE_BG);
    hbos_video_frame(state, 22, 150, 596, 205, 0x00404040U, 0x00404040U);

    if (state->history_count > HBOS_CONSOLE_ROWS) {
        start = state->history_count - HBOS_CONSOLE_ROWS;
    }
    for (i = start; i < state->history_count; i++) {
        line_y = (int)HBOS_CONSOLE_Y + (int)(i - start) * (int)HBOS_CONSOLE_CELL_H;
        hbos_video_text_limit(state, HBOS_CONSOLE_X, line_y, state->history[i], state->history_color[i],
                              HBOS_COLOR_CONSOLE_BG, 1, HBOS_CONSOLE_COLS);
    }
    line_y = (int)HBOS_CONSOLE_Y + (int)HBOS_CONSOLE_ROWS * (int)HBOS_CONSOLE_CELL_H;
    hbos_video_rect(state, HBOS_CONSOLE_X, line_y, (int)HBOS_CONSOLE_COLS * 8, 10, HBOS_COLOR_CONSOLE_BG);
    hbos_video_text(state, HBOS_CONSOLE_X, line_y, ">", HBOS_COLOR_OK, HBOS_COLOR_CONSOLE_BG, 1);
    hbos_video_text_limit(state, HBOS_CONSOLE_X + 16, line_y, state->input, HBOS_COLOR_CONSOLE_TEXT,
                          HBOS_COLOR_CONSOLE_BG, 1, HBOS_CONSOLE_COLS - 2);
    if ((state->frame_no & 16ULL) == 0ULL) {
        hbos_video_rect(state, HBOS_CONSOLE_X + 16 + (int)state->input_len * 8, line_y + 1, 7, 8, HBOS_COLOR_CONSOLE_TEXT);
    }
}

static void hbos_draw_about(hbos_state *state) {
    hbos_draw_window(state, 108, 42, 424, 78, "About HariboteOS compatibility layer");
    hbos_video_text(state, 124, 72, "HariboteOS 32-bit kernel/app model hosted in CLeonOS user space.",
                    HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
    hbos_video_text(state, 124, 90, "This build implements the UI, FAT view, console and app API stubs.",
                    HBOS_COLOR_TEXT, 0xFFFFFFFFU, 1);
}

static void hbos_draw_desktop_icons(hbos_state *state) {
    hbos_video_rect(state, 24, 24, 42, 34, 0x00FFFFE0U);
    hbos_video_frame(state, 24, 24, 42, 34, 0x00FFFFFFU, 0x00808040U);
    hbos_video_text(state, 17, 66, "APPS", HBOS_COLOR_CONSOLE_TEXT, 0xFFFFFFFFU, 1);
    hbos_video_rect(state, 94, 24, 42, 34, 0x00E0FFFFU);
    hbos_video_frame(state, 94, 24, 42, 34, 0x00FFFFFFU, 0x00408080U);
    hbos_video_text(state, 90, 66, "FILES", HBOS_COLOR_CONSOLE_TEXT, 0xFFFFFFFFU, 1);
}

void hbos_redraw(hbos_state *state) {
    if (state == (hbos_state *)0 || state->pixels == (hbos_u32 *)0) {
        return;
    }
    hbos_video_clear(state, HBOS_COLOR_BG);
    hbos_video_text(state, 12, 6, "HariboteOS", HBOS_COLOR_CONSOLE_TEXT, 0xFFFFFFFFU, 2);
    hbos_video_text(state, 176, 14, "Day 30 environment running under CLeonOS hbos", HBOS_COLOR_CONSOLE_TEXT,
                    0xFFFFFFFFU, 1);
    hbos_draw_desktop_icons(state);
    hbos_draw_console(state);
    if (state->show_about != 0) {
        hbos_draw_about(state);
    }
    hbos_draw_taskbar(state);
    state->dirty = 0;
}

static hbos_u64 hbos_min_u64(hbos_u64 a, hbos_u64 b) {
    return (a < b) ? a : b;
}

static int hbos_fit_size(hbos_u64 max_w, hbos_u64 max_h, hbos_u32 *out_w, hbos_u32 *out_h) {
    hbos_u64 w = HBOS_SCREEN_W;
    hbos_u64 h = HBOS_SCREEN_H;

    if (out_w == (hbos_u32 *)0 || out_h == (hbos_u32 *)0 || max_w == 0ULL || max_h == 0ULL) {
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

    if (w == 0ULL || h == 0ULL || w > 4096ULL || h > 4096ULL) {
        return 0;
    }

    *out_w = (hbos_u32)w;
    *out_h = (hbos_u32)h;
    return 1;
}

static int hbos_resize_present_buffer(hbos_state *state, hbos_u32 width, hbos_u32 height) {
    hbos_u64 count;
    hbos_u32 *pixels;

    if (state == (hbos_state *)0 || width == 0U || height == 0U) {
        return 0;
    }
    if (state->present_pixels != (hbos_u32 *)0 && state->present_w == width && state->present_h == height) {
        return 1;
    }

    count = (hbos_u64)width * (hbos_u64)height;
    if (count == 0ULL || count > (4096ULL * 4096ULL) || count > (((hbos_u64)-1) / sizeof(hbos_u32))) {
        return 0;
    }

    pixels = (hbos_u32 *)malloc((size_t)(count * sizeof(hbos_u32)));
    if (pixels == (hbos_u32 *)0) {
        return 0;
    }

    if (state->present_pixels != (hbos_u32 *)0) {
        free(state->present_pixels);
    }
    state->present_pixels = pixels;
    state->present_w = width;
    state->present_h = height;
    return 1;
}

static void hbos_scale_to_present(hbos_state *state, hbos_u32 width, hbos_u32 height) {
    hbos_u32 y;
    hbos_u32 x;

    if (state == (hbos_state *)0 || state->pixels == (hbos_u32 *)0 ||
        state->present_pixels == (hbos_u32 *)0 || width == 0U || height == 0U) {
        return;
    }

    for (y = 0U; y < height; y++) {
        hbos_u32 src_y = (hbos_u32)(((hbos_u64)y * (hbos_u64)HBOS_SCREEN_H) / (hbos_u64)height);
        if (src_y >= HBOS_SCREEN_H) {
            src_y = HBOS_SCREEN_H - 1U;
        }
        for (x = 0U; x < width; x++) {
            hbos_u32 src_x = (hbos_u32)(((hbos_u64)x * (hbos_u64)HBOS_SCREEN_W) / (hbos_u64)width);
            if (src_x >= HBOS_SCREEN_W) {
                src_x = HBOS_SCREEN_W - 1U;
            }
            state->present_pixels[(hbos_u64)y * (hbos_u64)width + (hbos_u64)x] =
                state->pixels[(hbos_u64)src_y * (hbos_u64)HBOS_SCREEN_W + (hbos_u64)src_x];
        }
    }
}

void hbos_present(hbos_state *state) {
    cleonos_fb_info fb;
    cleonos_display_info tty_display;
    cleonos_fb_blit_req req;
    hbos_u64 dst_x = 0ULL;
    hbos_u64 dst_y = 0ULL;
    hbos_u64 draw_area_w = HBOS_SCREEN_W;
    hbos_u64 draw_area_h = HBOS_SCREEN_H;
    hbos_u32 out_w = HBOS_SCREEN_W;
    hbos_u32 out_h = HBOS_SCREEN_H;
    hbos_u32 *src_pixels;

    if (state == (hbos_state *)0) {
        return;
    }

    if (state->terminal_only != 0) {
        while (state->terminal_flushed < state->history_count) {
            hbos_terminal_write(state->history[state->terminal_flushed]);
            hbos_terminal_write("\n");
            state->terminal_flushed++;
        }
        if (state->prompt_pending != 0 && state->running != 0) {
            hbos_terminal_write("> ");
            state->prompt_pending = 0;
        }
        state->dirty = 0;
        return;
    }

    if (state->pixels == (hbos_u32 *)0) {
        return;
    }

    if (state->dirty != 0) {
        hbos_redraw(state);
    }

    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return;
    }

    draw_area_w = fb.width;
    draw_area_h = fb.height;
    if (cleonos_sys_display_info(CLEONOS_DISPLAY_TARGET_TTY, &tty_display) != 0ULL &&
        tty_display.logical_width > 0ULL && tty_display.logical_height > 0ULL) {
        draw_area_w = hbos_min_u64(draw_area_w, tty_display.logical_width);
        draw_area_h = hbos_min_u64(draw_area_h, tty_display.logical_height);
        dst_x = (tty_display.physical_width > tty_display.logical_width) ?
            ((tty_display.physical_width - tty_display.logical_width) / 2ULL) : 0ULL;
        dst_y = (tty_display.physical_height > tty_display.logical_height) ?
            ((tty_display.physical_height - tty_display.logical_height) / 2ULL) : 0ULL;
    }

    if (hbos_fit_size(draw_area_w, draw_area_h, &out_w, &out_h) == 0) {
        return;
    }

    dst_x += (draw_area_w > (hbos_u64)out_w) ? ((draw_area_w - (hbos_u64)out_w) / 2ULL) : 0ULL;
    dst_y += (draw_area_h > (hbos_u64)out_h) ? ((draw_area_h - (hbos_u64)out_h) / 2ULL) : 0ULL;

    if (state->present_cleared == 0 || out_w != HBOS_SCREEN_W || out_h != HBOS_SCREEN_H) {
        (void)cleonos_sys_fb_clear(0ULL);
        state->present_cleared = 1;
    }

    src_pixels = state->pixels;
    if (out_w != HBOS_SCREEN_W || out_h != HBOS_SCREEN_H) {
        if (hbos_resize_present_buffer(state, out_w, out_h) == 0) {
            return;
        }
        hbos_scale_to_present(state, out_w, out_h);
        src_pixels = state->present_pixels;
    }

    req.pixels_ptr = (hbos_u64)(usize)src_pixels;
    req.src_width = out_w;
    req.src_height = out_h;
    req.src_pitch_bytes = (hbos_u64)out_w * HBOS_BYTES_PER_PIXEL;
    req.dst_x = dst_x;
    req.dst_y = dst_y;
    req.scale = 1ULL;
    (void)cleonos_sys_fb_blit(&req);
    state->frame_no++;
}
