#include "uwm.h"

#define UWM_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                         \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

#define UWM_COLOR_WHITE 0x00FFFFFFU
#define UWM_COLOR_WIN_BLUE 0x000078D7U
#define UWM_COLOR_DARKER 0x00181818U
#define UWM_COLOR_LIGHT_BG 0x00F3F3F3U
#define UWM_COLOR_PANEL 0x00FFFFFFU
#define UWM_COLOR_TEXT 0x00232323U
#define UWM_COLOR_MUTED 0x00666666U
#define UWM_COLOR_BORDER 0x00D0D0D0U

static int ush_uwm_work_bottom(const ush_uwm_session *sess) {
    int bottom;

    if (sess == (const ush_uwm_session *)0) {
        return USH_UWM_TOP_CLAMP_Y;
    }

    bottom = sess->screen_h - USH_UWM_TASKBAR_H;
    if (bottom < USH_UWM_TOP_CLAMP_Y) {
        bottom = USH_UWM_TOP_CLAMP_Y;
    }

    return bottom;
}

static ush_uwm_u32 ush_uwm_mix(ush_uwm_u32 a, ush_uwm_u32 b, int pos, int max_pos) {
    unsigned int ar;
    unsigned int ag;
    unsigned int ab;
    unsigned int br;
    unsigned int bg;
    unsigned int bb;
    unsigned int r;
    unsigned int g;
    unsigned int blue;
    unsigned int left;
    unsigned int right;

    if (max_pos <= 0) {
        return b;
    }

    pos = ush_uwm_clampi(pos, 0, max_pos);
    left = (unsigned int)(max_pos - pos);
    right = (unsigned int)pos;
    ar = (a >> 16U) & 0xFFU;
    ag = (a >> 8U) & 0xFFU;
    ab = a & 0xFFU;
    br = (b >> 16U) & 0xFFU;
    bg = (b >> 8U) & 0xFFU;
    bb = b & 0xFFU;
    r = ((ar * left) + (br * right)) / (unsigned int)max_pos;
    g = ((ag * left) + (bg * right)) / (unsigned int)max_pos;
    blue = ((ab * left) + (bb * right)) / (unsigned int)max_pos;
    return (ush_uwm_u32)((r << 16U) | (g << 8U) | blue);
}

static void ush_uwm_fill_rect(ush_uwm_window *win, int x, int y, int w, int h, ush_uwm_u32 color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (win == (ush_uwm_window *)0 || win->pixels == (ush_uwm_u32 *)0 || win->w <= 0 || win->h <= 0 || w <= 0 ||
        h <= 0) {
        return;
    }

    left = x;
    top = y;
    right = x + w;
    bottom = y + h;

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > win->w) {
        right = win->w;
    }
    if (bottom > win->h) {
        bottom = win->h;
    }

    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        u64 base = (u64)(unsigned int)row * (u64)(unsigned int)win->w;
        int col;

        if (base + (u64)(unsigned int)right > win->pixel_count) {
            return;
        }

        for (col = left; col < right; col++) {
            win->pixels[base + (u64)(unsigned int)col] = color;
        }
    }
}

static void ush_uwm_gradient_rect(ush_uwm_window *win, int x, int y, int w, int h, ush_uwm_u32 top,
                                  ush_uwm_u32 bottom) {
    int row;

    for (row = 0; row < h; row++) {
        ush_uwm_fill_rect(win, x, y + row, w, 1, ush_uwm_mix(top, bottom, row, h - 1));
    }
}

static void ush_uwm_stroke_rect(ush_uwm_window *win, int x, int y, int w, int h, ush_uwm_u32 color) {
    ush_uwm_fill_rect(win, x, y, w, 1, color);
    ush_uwm_fill_rect(win, x, y + h - 1, w, 1, color);
    ush_uwm_fill_rect(win, x, y, 1, h, color);
    ush_uwm_fill_rect(win, x + w - 1, y, 1, h, color);
}

static char ush_uwm_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }

    return ch;
}

static u64 ush_uwm_glyph_mask(char ch) {
    switch (ush_uwm_upper_char(ch)) {
    case 'A':
        return UWM_GLYPH7(14U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'B':
        return UWM_GLYPH7(30U, 17U, 17U, 30U, 17U, 17U, 30U);
    case 'C':
        return UWM_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return UWM_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return UWM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'F':
        return UWM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 16U);
    case 'G':
        return UWM_GLYPH7(14U, 17U, 16U, 23U, 17U, 17U, 15U);
    case 'H':
        return UWM_GLYPH7(17U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'I':
        return UWM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 31U);
    case 'J':
        return UWM_GLYPH7(1U, 1U, 1U, 1U, 17U, 17U, 14U);
    case 'K':
        return UWM_GLYPH7(17U, 18U, 20U, 24U, 20U, 18U, 17U);
    case 'L':
        return UWM_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'M':
        return UWM_GLYPH7(17U, 27U, 21U, 21U, 17U, 17U, 17U);
    case 'N':
        return UWM_GLYPH7(17U, 25U, 21U, 19U, 17U, 17U, 17U);
    case 'O':
        return UWM_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return UWM_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return UWM_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return UWM_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return UWM_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return UWM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    case 'U':
        return UWM_GLYPH7(17U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'V':
        return UWM_GLYPH7(17U, 17U, 17U, 17U, 17U, 10U, 4U);
    case 'W':
        return UWM_GLYPH7(17U, 17U, 17U, 21U, 21U, 21U, 10U);
    case 'X':
        return UWM_GLYPH7(17U, 17U, 10U, 4U, 10U, 17U, 17U);
    case 'Y':
        return UWM_GLYPH7(17U, 17U, 10U, 4U, 4U, 4U, 4U);
    case 'Z':
        return UWM_GLYPH7(31U, 1U, 2U, 4U, 8U, 16U, 31U);
    case '0':
        return UWM_GLYPH7(14U, 17U, 19U, 21U, 25U, 17U, 14U);
    case '1':
        return UWM_GLYPH7(4U, 12U, 4U, 4U, 4U, 4U, 14U);
    case '2':
        return UWM_GLYPH7(14U, 17U, 1U, 2U, 4U, 8U, 31U);
    case '3':
        return UWM_GLYPH7(30U, 1U, 1U, 14U, 1U, 1U, 30U);
    case '4':
        return UWM_GLYPH7(2U, 6U, 10U, 18U, 31U, 2U, 2U);
    case '5':
        return UWM_GLYPH7(31U, 16U, 16U, 30U, 1U, 1U, 30U);
    case '6':
        return UWM_GLYPH7(14U, 16U, 16U, 30U, 17U, 17U, 14U);
    case '7':
        return UWM_GLYPH7(31U, 1U, 2U, 4U, 8U, 8U, 8U);
    case '8':
        return UWM_GLYPH7(14U, 17U, 17U, 14U, 17U, 17U, 14U);
    case '9':
        return UWM_GLYPH7(14U, 17U, 17U, 15U, 1U, 1U, 14U);
    case '-':
        return UWM_GLYPH7(0U, 0U, 0U, 31U, 0U, 0U, 0U);
    case '_':
        return UWM_GLYPH7(0U, 0U, 0U, 0U, 0U, 0U, 31U);
    case '.':
        return UWM_GLYPH7(0U, 0U, 0U, 0U, 0U, 12U, 12U);
    case ':':
        return UWM_GLYPH7(0U, 12U, 12U, 0U, 12U, 12U, 0U);
    case '/':
        return UWM_GLYPH7(1U, 1U, 2U, 4U, 8U, 16U, 16U);
    case '+':
        return UWM_GLYPH7(0U, 4U, 4U, 31U, 4U, 4U, 0U);
    case '=':
        return UWM_GLYPH7(0U, 0U, 31U, 0U, 31U, 0U, 0U);
    case '^':
        return UWM_GLYPH7(4U, 10U, 17U, 0U, 0U, 0U, 0U);
    case '|':
        return UWM_GLYPH7(4U, 4U, 4U, 4U, 4U, 4U, 4U);
    default:
        return 0ULL;
    }
}

static void ush_uwm_draw_char(ush_uwm_window *win, int x, int y, char ch, int scale, ush_uwm_u32 color) {
    u64 mask = ush_uwm_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }

    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit_index = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit_index)) != 0ULL) {
                ush_uwm_fill_rect(win, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void ush_uwm_draw_text_limit(ush_uwm_window *win, int x, int y, const char *text, int scale, ush_uwm_u32 color,
                                    int max_x) {
    int cursor_x = x;

    if (win == (ush_uwm_window *)0 || text == (const char *)0 || scale <= 0) {
        return;
    }

    if (max_x <= 0 || max_x > win->w) {
        max_x = win->w;
    }

    while (*text != 0 && cursor_x + (5 * scale) <= max_x) {
        if (*text != ' ') {
            ush_uwm_draw_char(win, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

static void ush_uwm_draw_text(ush_uwm_window *win, int x, int y, const char *text, int scale, ush_uwm_u32 color) {
    ush_uwm_draw_text_limit(win, x, y, text, scale, color, win != (ush_uwm_window *)0 ? win->w : 0);
}

static void ush_uwm_draw_windows_logo(ush_uwm_window *win, int x, int y, int size, ush_uwm_u32 color) {
    int half = size / 2;
    int gap = (size >= 20) ? 2 : 1;

    ush_uwm_fill_rect(win, x, y, half - gap, half - gap, color);
    ush_uwm_fill_rect(win, x + half + gap, y, half - gap, half - gap, color);
    ush_uwm_fill_rect(win, x, y + half + gap, half - gap, half - gap, color);
    ush_uwm_fill_rect(win, x + half + gap, y + half + gap, half - gap, half - gap, color);
}

static void ush_uwm_draw_control_button(ush_uwm_window *win, int x, int active, int kind) {
    ush_uwm_u32 bg = (kind == 2) ? 0x00E81123U : (active != 0 ? 0x001A5EA0U : 0x00E5E5E5U);
    ush_uwm_u32 fg = (kind == 2 || active != 0) ? UWM_COLOR_WHITE : UWM_COLOR_TEXT;
    int cy = USH_UWM_TITLE_H / 2;
    int cx = x + (USH_UWM_CONTROL_W / 2);

    ush_uwm_fill_rect(win, x, 0, USH_UWM_CONTROL_W, USH_UWM_TITLE_H, bg);
    if (kind == 0) {
        ush_uwm_fill_rect(win, cx - 6, cy + 4, 12, 1, fg);
    } else if (kind == 1) {
        ush_uwm_fill_rect(win, cx - 4, cy - 6, 8, 2, fg);
        ush_uwm_fill_rect(win, cx - 1, cy - 4, 2, 9, fg);
        ush_uwm_fill_rect(win, cx - 6, cy + 4, 12, 2, fg);
    } else {
        int i;
        for (i = 0; i < 11; i++) {
            ush_uwm_fill_rect(win, cx - 5 + i, cy - 5 + i, 1, 1, fg);
            ush_uwm_fill_rect(win, cx + 5 - i, cy - 5 + i, 1, 1, fg);
        }
    }
}

static void ush_uwm_draw_window_controls(ush_uwm_window *win, int active) {
    int close_x;
    int pin_x;
    int min_x;

    if (win == (ush_uwm_window *)0 || win->w < (USH_UWM_CONTROL_W * 3) + 16) {
        return;
    }

    close_x = win->w - USH_UWM_CONTROL_W;
    min_x = close_x - USH_UWM_CONTROL_W;
    pin_x = min_x - USH_UWM_CONTROL_W;
    ush_uwm_draw_control_button(win, pin_x, active, 1);
    ush_uwm_draw_control_button(win, min_x, active, 0);
    ush_uwm_draw_control_button(win, close_x, active, 2);
}

static void ush_uwm_draw_button(ush_uwm_window *win, int x, int y, int w, int h, const char *label, ush_uwm_u32 bg,
                                ush_uwm_u32 fg, int active) {
    ush_uwm_fill_rect(win, x, y, w, h, bg);
    if (active != 0) {
        ush_uwm_fill_rect(win, x, y + h - 3, w, 3, UWM_COLOR_WIN_BLUE);
    }
    ush_uwm_draw_text_limit(win, x + 10, y + ((h - 7) / 2), label, 1, fg, x + w - 8);
}

static void ush_uwm_render_files(ush_uwm_window *win) {
    int y;

    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H, 92, win->h - USH_UWM_TITLE_H, 0x00F7F7F7U);
    ush_uwm_fill_rect(win, 92, USH_UWM_TITLE_H, 1, win->h - USH_UWM_TITLE_H, UWM_COLOR_BORDER);
    ush_uwm_draw_text(win, 12, USH_UWM_TITLE_H + 16, "QUICK ACCESS", 1, UWM_COLOR_MUTED);
    ush_uwm_draw_text(win, 18, USH_UWM_TITLE_H + 40, "DESKTOP", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 18, USH_UWM_TITLE_H + 62, "SYSTEM", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 18, USH_UWM_TITLE_H + 84, "TEMP", 1, UWM_COLOR_TEXT);

    ush_uwm_draw_text(win, 112, USH_UWM_TITLE_H + 16, "THIS PC", 2, UWM_COLOR_TEXT);
    ush_uwm_fill_rect(win, 112, USH_UWM_TITLE_H + 48, win->w - 132, 34, UWM_COLOR_PANEL);
    ush_uwm_stroke_rect(win, 112, USH_UWM_TITLE_H + 48, win->w - 132, 34, UWM_COLOR_BORDER);
    ush_uwm_draw_text(win, 124, USH_UWM_TITLE_H + 60, "C:/ CLEONOS LOCAL DISK", 1, UWM_COLOR_TEXT);
    ush_uwm_fill_rect(win, 124, USH_UWM_TITLE_H + 72, win->w - 184, 4, 0x00D9D9D9U);
    ush_uwm_fill_rect(win, 124, USH_UWM_TITLE_H + 72, (win->w - 184) / 2, 4, UWM_COLOR_WIN_BLUE);

    y = USH_UWM_TITLE_H + 104;
    while (y + 22 < win->h - 14) {
        ush_uwm_fill_rect(win, 112, y, win->w - 132, 1, 0x00E6E6E6U);
        y += 28;
    }
    ush_uwm_draw_text(win, 118, USH_UWM_TITLE_H + 110, "SYSTEM", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 118, USH_UWM_TITLE_H + 138, "SHELL", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 118, USH_UWM_TITLE_H + 166, "TEMP", 1, UWM_COLOR_TEXT);
}

static void ush_uwm_render_editor(ush_uwm_window *win) {
    int y;

    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H, win->w, 24, 0x00F9F9F9U);
    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H + 23, win->w, 1, UWM_COLOR_BORDER);
    ush_uwm_draw_text(win, 12, USH_UWM_TITLE_H + 8, "FILE  EDIT  VIEW  HELP", 1, UWM_COLOR_TEXT);
    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H + 24, 46, win->h - USH_UWM_TITLE_H - 24, 0x00F0F0F0U);
    ush_uwm_fill_rect(win, 46, USH_UWM_TITLE_H + 24, 1, win->h - USH_UWM_TITLE_H - 24, 0x00DDDDDDU);

    for (y = 0; y < 7; y++) {
        char label[4];
        label[0] = (char)('1' + y);
        label[1] = 0;
        ush_uwm_draw_text(win, 18, USH_UWM_TITLE_H + 42 + (y * 18), label, 1, UWM_COLOR_MUTED);
    }

    ush_uwm_draw_text(win, 62, USH_UWM_TITLE_H + 42, "CLEONOS UWM REWRITE", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 62, USH_UWM_TITLE_H + 60, "PIXEL RENDERER ONLINE", 1, 0x00008000U);
    ush_uwm_draw_text(win, 62, USH_UWM_TITLE_H + 78, "WINDOWS 10 STYLE SHELL", 1, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 62, USH_UWM_TITLE_H + 96, "DRAG RESIZE MINIMIZE CLOSE", 1, UWM_COLOR_TEXT);
    ush_uwm_fill_rect(win, 62, USH_UWM_TITLE_H + 120, 92, 2, UWM_COLOR_WIN_BLUE);
}

static void ush_uwm_render_browser(ush_uwm_window *win) {
    int card_w;

    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H, win->w, 42, 0x00F7F7F7U);
    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H + 41, win->w, 1, UWM_COLOR_BORDER);
    ush_uwm_fill_rect(win, 14, USH_UWM_TITLE_H + 10, win->w - 28, 22, UWM_COLOR_WHITE);
    ush_uwm_stroke_rect(win, 14, USH_UWM_TITLE_H + 10, win->w - 28, 22, UWM_COLOR_BORDER);
    ush_uwm_draw_text_limit(win, 24, USH_UWM_TITLE_H + 17, "HTTP://EXAMPLE.COM", 1, UWM_COLOR_MUTED, win->w - 24);

    ush_uwm_draw_text(win, 22, USH_UWM_TITLE_H + 66, "WELCOME TO CLEONOS", 2, UWM_COLOR_TEXT);
    ush_uwm_draw_text(win, 24, USH_UWM_TITLE_H + 94, "NETWORK AND HTML DEMOS LIVE HERE", 1, UWM_COLOR_MUTED);
    card_w = (win->w - 58) / 2;
    if (card_w < 80) {
        card_w = 80;
    }
    ush_uwm_fill_rect(win, 22, USH_UWM_TITLE_H + 122, card_w, 54, 0x00EAF4FFU);
    ush_uwm_stroke_rect(win, 22, USH_UWM_TITLE_H + 122, card_w, 54, 0x00B7D8F4U);
    ush_uwm_draw_text(win, 34, USH_UWM_TITLE_H + 142, "HTTPGET", 1, UWM_COLOR_TEXT);
    ush_uwm_fill_rect(win, 36 + card_w, USH_UWM_TITLE_H + 122, card_w, 54, 0x00EAF7EAU);
    ush_uwm_stroke_rect(win, 36 + card_w, USH_UWM_TITLE_H + 122, card_w, 54, 0x00B7E0B7U);
    ush_uwm_draw_text(win, 48 + card_w, USH_UWM_TITLE_H + 142, "NSLOOKUP", 1, UWM_COLOR_TEXT);
}

static void ush_uwm_render_app_window(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;
    int active;
    ush_uwm_u32 title_bg;
    ush_uwm_u32 title_fg;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    active = (sess->active_window == index && win->minimized == 0 && win->closed == 0) ? 1 : 0;
    title_bg = (active != 0) ? win->accent : 0x00F3F3F3U;
    title_fg = (active != 0) ? UWM_COLOR_WHITE : UWM_COLOR_TEXT;

    ush_uwm_fill_rect(win, 0, 0, win->w, win->h, UWM_COLOR_LIGHT_BG);
    ush_uwm_fill_rect(win, 0, 0, win->w, USH_UWM_TITLE_H, title_bg);
    ush_uwm_fill_rect(win, 0, USH_UWM_TITLE_H, win->w, 1, UWM_COLOR_BORDER);
    ush_uwm_draw_text_limit(win, 12, 12, win->title, 1, title_fg, win->w - (USH_UWM_CONTROL_W * 3) - 8);
    if (win->topmost != 0) {
        ush_uwm_draw_text(win, win->w - (USH_UWM_CONTROL_W * 3) - 18, 12, "^", 1, title_fg);
    }
    ush_uwm_draw_window_controls(win, active);

    if (index == 0) {
        ush_uwm_render_files(win);
    } else if (index == 1) {
        ush_uwm_render_editor(win);
    } else {
        ush_uwm_render_browser(win);
    }

    ush_uwm_fill_rect(win, win->w - 14, win->h - 3, 11, 1, UWM_COLOR_MUTED);
    ush_uwm_fill_rect(win, win->w - 10, win->h - 7, 7, 1, UWM_COLOR_MUTED);
    ush_uwm_fill_rect(win, win->w - 6, win->h - 11, 3, 1, UWM_COLOR_MUTED);
}

static void ush_uwm_render_taskbar(ush_uwm_session *sess) {
    ush_uwm_window *taskbar;
    int search_w;
    int app_x;
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    taskbar = &sess->windows[USH_UWM_TASKBAR_INDEX];
    ush_uwm_gradient_rect(taskbar, 0, 0, taskbar->w, taskbar->h, 0x00252525U, 0x001A1A1AU);
    ush_uwm_fill_rect(taskbar, 0, 0, taskbar->w, 1, 0x004A4A4AU);
    ush_uwm_fill_rect(taskbar, 0, 0, USH_UWM_TASKBAR_START_W, taskbar->h,
                      sess->start_open ? UWM_COLOR_WIN_BLUE : UWM_COLOR_DARKER);
    ush_uwm_draw_windows_logo(taskbar, 15, 12, 16, UWM_COLOR_WHITE);

    search_w = USH_UWM_TASKBAR_SEARCH_W;
    if (taskbar->w < 720) {
        search_w = 128;
    }
    if (taskbar->w < 520) {
        search_w = 0;
    }

    app_x = USH_UWM_TASKBAR_START_W + 8;
    if (search_w > 0) {
        ush_uwm_fill_rect(taskbar, app_x, 6, search_w, taskbar->h - 12, 0x00303030U);
        ush_uwm_stroke_rect(taskbar, app_x, 6, search_w, taskbar->h - 12, 0x00484848U);
        ush_uwm_draw_text_limit(taskbar, app_x + 12, 17, "TYPE HERE TO SEARCH", 1, 0x00C8C8C8U, app_x + search_w - 8);
        app_x += search_w + 10;
    }

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        ush_uwm_window *app = &sess->windows[i];
        ush_uwm_u32 bg = 0x00282828U;
        ush_uwm_u32 fg = 0x00EAEAEAU;
        int active = 0;

        if (app_x + USH_UWM_TASKBAR_BUTTON_W > taskbar->w - 98) {
            break;
        }

        if (app->closed != 0) {
            bg = 0x001F1F1FU;
            fg = 0x008F8F8FU;
        } else if (app->minimized != 0) {
            bg = 0x002F2F2FU;
        } else if (sess->active_window == i) {
            bg = 0x00383838U;
            active = 1;
        }

        ush_uwm_draw_button(taskbar, app_x, 5, USH_UWM_TASKBAR_BUTTON_W, taskbar->h - 10, app->title, bg, fg, active);
        app_x += USH_UWM_TASKBAR_BUTTON_W + USH_UWM_TASKBAR_BUTTON_GAP;
    }

    if (taskbar->w > 260) {
        ush_uwm_draw_text(taskbar, taskbar->w - 86, 11, "CLKS", 1, UWM_COLOR_WHITE);
        ush_uwm_draw_text(taskbar, taskbar->w - 86, 24, "UWM", 1, 0x00CFCFCFU);
    }
}

static void ush_uwm_render_start(ush_uwm_session *sess) {
    ush_uwm_window *start;
    int i;
    const char *labels[USH_UWM_APP_COUNT] = {"FILE EXPLORER", "NOTEPAD", "EDGE"};

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    start = &sess->windows[USH_UWM_START_INDEX];
    ush_uwm_gradient_rect(start, 0, 0, start->w, start->h, 0x00292929U, 0x001E1E1EU);
    ush_uwm_fill_rect(start, 0, 0, 52, start->h, 0x00191919U);
    ush_uwm_draw_windows_logo(start, 16, start->h - 34, 18, UWM_COLOR_WHITE);
    ush_uwm_draw_text(start, 68, 20, "CLEONOS", 2, UWM_COLOR_WHITE);
    ush_uwm_draw_text(start, 70, 48, "START", 1, 0x00BDBDBDU);

    for (i = 0; i < (int)USH_UWM_APP_COUNT; i++) {
        int y = 78 + (i * 44);
        ush_uwm_u32 bg = (sess->active_window == i && sess->windows[i].closed == 0 && sess->windows[i].minimized == 0)
                             ? 0x003B3B3BU
                             : 0x00272727U;
        if (y + 34 > start->h - 76) {
            break;
        }
        ush_uwm_fill_rect(start, 66, y, start->w - 82, 34, bg);
        ush_uwm_fill_rect(start, 66, y, 4, 34, sess->windows[i].accent);
        ush_uwm_draw_text_limit(start, 82, y + 10, labels[i], 1, UWM_COLOR_WHITE, start->w - 12);
    }

    if (start->w > 248 && start->h > 260) {
        int tile_y = start->h - 68;
        int tile_w = (start->w - 88) / 2;
        ush_uwm_fill_rect(start, 66, tile_y, tile_w, 48, UWM_COLOR_WIN_BLUE);
        ush_uwm_draw_text(start, 78, tile_y + 18, "SETTINGS", 1, UWM_COLOR_WHITE);
        ush_uwm_fill_rect(start, 74 + tile_w, tile_y, tile_w, 48, 0x0000A300U);
        ush_uwm_draw_text(start, 86 + tile_w, tile_y + 18, "TERMINAL", 1, UWM_COLOR_WHITE);
    }
}

void ush_uwm_render_window(ush_uwm_session *sess, int index) {
    if (sess == (ush_uwm_session *)0 || ush_uwm_window_index_valid(index) == 0) {
        return;
    }

    if (sess->windows[index].pixels == (ush_uwm_u32 *)0) {
        return;
    }

    if (sess->windows[index].kind == USH_UWM_KIND_TASKBAR) {
        ush_uwm_render_taskbar(sess);
    } else if (sess->windows[index].kind == USH_UWM_KIND_START) {
        ush_uwm_render_start(sess);
    } else {
        ush_uwm_render_app_window(sess, index);
    }
    sess->windows[index].dirty = 0;
}

void ush_uwm_refresh_window(ush_uwm_session *sess, int index) {
    if (sess == (ush_uwm_session *)0 || ush_uwm_window_index_valid(index) == 0 || sess->windows[index].alive == 0) {
        return;
    }

    ush_uwm_render_window(sess, index);
    (void)ush_uwm_present_window(&sess->windows[index]);
}

void ush_uwm_refresh_taskbar(ush_uwm_session *sess) {
    ush_uwm_refresh_window(sess, USH_UWM_TASKBAR_INDEX);
}

int ush_uwm_alloc_pixels(ush_uwm_window *win) {
    return (win == (ush_uwm_window *)0) ? 0 : ush_uwm_replace_pixels(win, win->w, win->h);
}

int ush_uwm_replace_pixels(ush_uwm_window *win, int width, int height) {
    u64 count;
    u64 bytes;
    ush_uwm_u32 *new_pixels;

    if (win == (ush_uwm_window *)0 || width <= 0 || height <= 0) {
        return 0;
    }

    count = (u64)(unsigned int)width * (u64)(unsigned int)height;
    if (count == 0ULL || count > (((u64)-1) / 4ULL)) {
        return 0;
    }

    bytes = count * 4ULL;
    new_pixels = (ush_uwm_u32 *)malloc((size_t)bytes);
    if (new_pixels == (ush_uwm_u32 *)0) {
        return 0;
    }

    if (win->pixels != (ush_uwm_u32 *)0) {
        free(win->pixels);
    }

    win->pixels = new_pixels;
    win->pixel_count = count;
    win->w = width;
    win->h = height;
    ush_zero(win->pixels, bytes);
    win->dirty = 1;
    return 1;
}

int ush_uwm_create_window(ush_uwm_window *win) {
    cleonos_wm_create_req req;

    if (win == (ush_uwm_window *)0) {
        return 0;
    }

    req.x = (u64)(i64)win->x;
    req.y = (u64)(i64)win->y;
    req.width = (u64)(unsigned int)win->w;
    req.height = (u64)(unsigned int)win->h;
    req.flags = (win->topmost != 0) ? USH_UWM_WM_FLAG_TOPMOST : 0ULL;
    win->id = cleonos_sys_wm_create(&req);
    if (win->id == 0ULL) {
        return 0;
    }

    win->alive = 1;
    return 1;
}

int ush_uwm_present_window(const ush_uwm_window *win) {
    cleonos_wm_present_req req;

    if (win == (const ush_uwm_window *)0 || win->alive == 0 || win->id == 0ULL || win->pixels == (ush_uwm_u32 *)0) {
        return 0;
    }

    req.window_id = win->id;
    req.pixels_ptr = (u64)(usize)win->pixels;
    req.src_width = (u64)(unsigned int)win->w;
    req.src_height = (u64)(unsigned int)win->h;
    req.src_pitch_bytes = (u64)(unsigned int)win->w * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

void ush_uwm_destroy_kernel_window(ush_uwm_window *win) {
    if (win == (ush_uwm_window *)0) {
        return;
    }

    if (win->id != 0ULL) {
        (void)cleonos_sys_wm_destroy(win->id);
    }

    win->id = 0ULL;
    win->alive = 0;
}

void ush_uwm_destroy_window(ush_uwm_window *win) {
    if (win == (ush_uwm_window *)0) {
        return;
    }

    ush_uwm_destroy_kernel_window(win);
    if (win->pixels != (ush_uwm_u32 *)0) {
        free(win->pixels);
    }

    win->pixels = (ush_uwm_u32 *)0;
    win->pixel_count = 0ULL;
    win->minimized = 0;
    win->closed = 1;
    win->dirty = 1;
}

int ush_uwm_window_move_clamped(ush_uwm_session *sess, int index, int target_x, int target_y) {
    ush_uwm_window *win;
    cleonos_wm_move_req req;
    int max_x;
    int max_y;
    int new_x;
    int new_y;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return 0;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->id == 0ULL || win->closed != 0 || win->minimized != 0) {
        return 0;
    }

    max_x = sess->screen_w - win->w;
    max_y = ush_uwm_work_bottom(sess) - win->h;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < USH_UWM_TOP_CLAMP_Y) {
        max_y = USH_UWM_TOP_CLAMP_Y;
    }

    new_x = ush_uwm_clampi(target_x, 0, max_x);
    new_y = ush_uwm_clampi(target_y, USH_UWM_TOP_CLAMP_Y, max_y);
    if (new_x == win->x && new_y == win->y) {
        return 1;
    }

    req.window_id = win->id;
    req.x = (u64)(i64)new_x;
    req.y = (u64)(i64)new_y;
    if (cleonos_sys_wm_move(&req) == 0ULL) {
        return 0;
    }

    win->x = new_x;
    win->y = new_y;
    return 1;
}

int ush_uwm_window_resize(ush_uwm_session *sess, int index, int target_w, int target_h) {
    ush_uwm_window *win;
    cleonos_wm_resize_req req;
    ush_uwm_u32 *new_pixels;
    u64 count;
    u64 bytes;
    int max_w;
    int max_h;
    int new_w;
    int new_h;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return 0;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->id == 0ULL || win->closed != 0 || win->minimized != 0) {
        return 0;
    }

    max_w = sess->screen_w - win->x;
    max_h = ush_uwm_work_bottom(sess) - win->y;
    if (max_w < USH_UWM_MIN_WINDOW_W) {
        max_w = USH_UWM_MIN_WINDOW_W;
    }
    if (max_h < USH_UWM_MIN_WINDOW_H) {
        max_h = USH_UWM_MIN_WINDOW_H;
    }

    new_w = ush_uwm_clampi(target_w, USH_UWM_MIN_WINDOW_W, max_w);
    new_h = ush_uwm_clampi(target_h, USH_UWM_MIN_WINDOW_H, max_h);
    if (new_w == win->w && new_h == win->h) {
        return 1;
    }

    count = (u64)(unsigned int)new_w * (u64)(unsigned int)new_h;
    if (count == 0ULL || count > (((u64)-1) / 4ULL)) {
        return 0;
    }

    bytes = count * 4ULL;
    new_pixels = (ush_uwm_u32 *)malloc((size_t)bytes);
    if (new_pixels == (ush_uwm_u32 *)0) {
        return 0;
    }
    ush_zero(new_pixels, bytes);

    req.window_id = win->id;
    req.width = (u64)(unsigned int)new_w;
    req.height = (u64)(unsigned int)new_h;
    if (cleonos_sys_wm_resize(&req) == 0ULL) {
        free(new_pixels);
        return 0;
    }

    if (win->pixels != (ush_uwm_u32 *)0) {
        free(win->pixels);
    }
    win->pixels = new_pixels;
    win->pixel_count = count;
    win->w = new_w;
    win->h = new_h;
    win->dirty = 1;

    ush_uwm_render_window(sess, index);
    (void)ush_uwm_present_window(win);
    return 1;
}

void ush_uwm_set_active(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;
    int old_active;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->id == 0ULL || win->closed != 0 || win->minimized != 0) {
        return;
    }

    old_active = sess->active_window;
    if (cleonos_sys_wm_set_focus(win->id) != 0ULL) {
        sess->active_window = index;
        if (ush_uwm_app_index_valid(old_active) != 0 && old_active != index) {
            ush_uwm_refresh_window(sess, old_active);
        }
        ush_uwm_refresh_window(sess, index);
        ush_uwm_refresh_taskbar(sess);
    }
}

void ush_uwm_focus_next(ush_uwm_session *sess) {
    int start;
    int i;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    start = (ush_uwm_app_index_valid(sess->active_window) != 0) ? sess->active_window : 0;
    for (i = 1; i <= (int)USH_UWM_APP_COUNT; i++) {
        int idx = (start + i) % (int)USH_UWM_APP_COUNT;
        if (sess->windows[idx].alive != 0 && sess->windows[idx].minimized == 0 && sess->windows[idx].closed == 0) {
            ush_uwm_set_active(sess, idx);
            return;
        }
    }

    sess->active_window = -1;
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_minimize_window(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    if (win->alive == 0 || win->closed != 0) {
        return;
    }

    ush_uwm_destroy_kernel_window(win);
    win->minimized = 1;
    if (sess->active_window == index) {
        ush_uwm_focus_next(sess);
    }
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_close_window(ush_uwm_session *sess, int index) {
    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    ush_uwm_destroy_kernel_window(&sess->windows[index]);
    sess->windows[index].closed = 1;
    sess->windows[index].minimized = 0;
    if (sess->active_window == index) {
        ush_uwm_focus_next(sess);
    }
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_restore_window(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    if (win->alive != 0) {
        win->closed = 0;
        win->minimized = 0;
        ush_uwm_set_active(sess, index);
        return;
    }

    if (win->pixels == (ush_uwm_u32 *)0 && ush_uwm_alloc_pixels(win) == 0) {
        return;
    }

    win->closed = 0;
    win->minimized = 0;
    ush_uwm_render_window(sess, index);
    if (ush_uwm_create_window(win) == 0) {
        win->closed = 1;
        return;
    }
    (void)ush_uwm_present_window(win);
    ush_uwm_set_active(sess, index);
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_toggle_topmost(ush_uwm_session *sess, int index) {
    ush_uwm_window *win;
    u64 flags;

    if (sess == (ush_uwm_session *)0 || ush_uwm_app_index_valid(index) == 0) {
        return;
    }

    win = &sess->windows[index];
    if (win->id == 0ULL || win->alive == 0) {
        return;
    }

    win->topmost = (win->topmost == 0) ? 1 : 0;
    flags = (win->topmost != 0) ? USH_UWM_WM_FLAG_TOPMOST : 0ULL;
    (void)cleonos_sys_wm_set_flags(win->id, flags);
    ush_uwm_refresh_window(sess, index);
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_close_start(ush_uwm_session *sess) {
    ush_uwm_window *start;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    start = &sess->windows[USH_UWM_START_INDEX];
    if (start->alive != 0) {
        ush_uwm_destroy_kernel_window(start);
    }
    sess->start_open = 0;
    start->closed = 1;
    if (ush_uwm_app_index_valid(sess->active_window) != 0 && sess->windows[sess->active_window].alive != 0) {
        (void)cleonos_sys_wm_set_focus(sess->windows[sess->active_window].id);
    }
    ush_uwm_refresh_taskbar(sess);
}

void ush_uwm_toggle_start(ush_uwm_session *sess) {
    ush_uwm_window *start;

    if (sess == (ush_uwm_session *)0) {
        return;
    }

    start = &sess->windows[USH_UWM_START_INDEX];
    if (start->alive != 0) {
        ush_uwm_close_start(sess);
        return;
    }

    if (start->pixels == (ush_uwm_u32 *)0 && ush_uwm_alloc_pixels(start) == 0) {
        return;
    }

    sess->start_open = 1;
    start->closed = 0;
    ush_uwm_render_window(sess, USH_UWM_START_INDEX);
    if (ush_uwm_create_window(start) == 0) {
        sess->start_open = 0;
        start->closed = 1;
        return;
    }
    (void)ush_uwm_present_window(start);
    (void)cleonos_sys_wm_set_focus(start->id);
    ush_uwm_refresh_taskbar(sess);
}
