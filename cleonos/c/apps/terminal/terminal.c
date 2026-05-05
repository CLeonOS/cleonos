#include "terminal.h"

#define TERM_TTY_DISPLAY 1ULL
#define TERM_TOP_CLAMP_Y 24
#define TERM_TASKBAR_H 48
#define TERM_TITLE_H 32
#define TERM_BOTTOM_H 30
#define TERM_CONTROL_W 46
#define TERM_RESIZE_GRIP 18
#define TERM_MIN_W 360
#define TERM_MIN_H 220
#define TERM_DEFAULT_W 720
#define TERM_DEFAULT_H 420
#define TERM_CANVAS_MAX_PIXELS (1280ULL * 800ULL)
#define TERM_LINES 128U
#define TERM_LINE_MAX 160U
#define TERM_INPUT_MAX 160U
#define TERM_ANSI_MAX 95U
#define TERM_EVENT_BUDGET 128ULL
#define TERM_IDLE_SPINS 24
#define TERM_STYLE_NONE 0U
#define TERM_STYLE_BOLD 1U
#define TERM_STYLE_UNDERLINE 2U

#define TERM_COLOR_WHITE 0x00FFFFFFU
#define TERM_COLOR_WIN_BLUE 0x000078D7U
#define TERM_COLOR_CLOSE 0x00E81123U
#define TERM_COLOR_TITLE_INACTIVE 0x00F3F3F3U
#define TERM_COLOR_TEXT 0x00232323U
#define TERM_COLOR_MUTED 0x00666666U
#define TERM_COLOR_BORDER 0x00D0D0D0U
#define TERM_COLOR_CONTROL_INACTIVE 0x00E5E5E5U
#define TERM_COLOR_CONTROL_ACTIVE 0x001A5EA0U
#define TERM_COLOR_BG 0x000C0C0CU
#define TERM_COLOR_BAR 0x00111111U
#define TERM_COLOR_DEFAULT 0x00DCDCDCU
#define TERM_COLOR_PROMPT 0x0086D98AU

#define TERM_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                        \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

typedef unsigned int term_u32;
typedef unsigned char term_u8;

typedef struct term_app {
    int screen_w;
    int screen_h;
    int x;
    int y;
    int w;
    int h;
    int restore_x;
    int restore_y;
    int restore_w;
    int restore_h;
    int running;
    int focused;
    int dragging;
    int drag_dx;
    int drag_dy;
    int resizing;
    int resize_start_x;
    int resize_start_y;
    int resize_start_w;
    int resize_start_h;
    int maximized;
    u64 window_id;
    u64 old_tty;
    int tty_switched;
    term_u32 *pixels;
    u64 pixel_count;
    char cwd[USH_PATH_MAX];
    char input[TERM_INPUT_MAX];
    u64 input_len;
    char lines[TERM_LINES][TERM_LINE_MAX];
    term_u32 cell_fg[TERM_LINES][TERM_LINE_MAX];
    term_u32 cell_bg[TERM_LINES][TERM_LINE_MAX];
    term_u8 cell_style[TERM_LINES][TERM_LINE_MAX];
    u64 line_count;
    u64 cursor_row;
    u64 cursor_col;
    u64 saved_row;
    u64 saved_col;
    term_u32 current_fg;
    term_u32 current_bg;
    term_u8 current_style;
    int ansi_bold;
    int ansi_underline;
    int ansi_inverse;
    int ansi_state;
    char ansi_buf[TERM_ANSI_MAX];
    u64 ansi_len;
} term_app;

static int term_clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int term_u64_as_i32(u64 raw) {
    return (int)(i64)raw;
}

static void term_append_to(char *dst, u64 dst_size, const char *src) {
    u64 len;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }

    len = ush_strlen(dst);
    while (src[i] != '\0' && len + 1ULL < dst_size) {
        dst[len++] = src[i++];
    }
    dst[len] = '\0';
}

static char term_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static u64 term_glyph_mask(char ch) {
    switch (term_upper_char(ch)) {
    case 'A':
        return TERM_GLYPH7(14U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'B':
        return TERM_GLYPH7(30U, 17U, 17U, 30U, 17U, 17U, 30U);
    case 'C':
        return TERM_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return TERM_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return TERM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'F':
        return TERM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 16U);
    case 'G':
        return TERM_GLYPH7(14U, 17U, 16U, 23U, 17U, 17U, 15U);
    case 'H':
        return TERM_GLYPH7(17U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'I':
        return TERM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 31U);
    case 'J':
        return TERM_GLYPH7(1U, 1U, 1U, 1U, 17U, 17U, 14U);
    case 'K':
        return TERM_GLYPH7(17U, 18U, 20U, 24U, 20U, 18U, 17U);
    case 'L':
        return TERM_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'M':
        return TERM_GLYPH7(17U, 27U, 21U, 21U, 17U, 17U, 17U);
    case 'N':
        return TERM_GLYPH7(17U, 25U, 21U, 19U, 17U, 17U, 17U);
    case 'O':
        return TERM_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return TERM_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return TERM_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return TERM_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return TERM_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return TERM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    case 'U':
        return TERM_GLYPH7(17U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'V':
        return TERM_GLYPH7(17U, 17U, 17U, 17U, 17U, 10U, 4U);
    case 'W':
        return TERM_GLYPH7(17U, 17U, 17U, 21U, 21U, 21U, 10U);
    case 'X':
        return TERM_GLYPH7(17U, 17U, 10U, 4U, 10U, 17U, 17U);
    case 'Y':
        return TERM_GLYPH7(17U, 17U, 10U, 4U, 4U, 4U, 4U);
    case 'Z':
        return TERM_GLYPH7(31U, 1U, 2U, 4U, 8U, 16U, 31U);
    case '0':
        return TERM_GLYPH7(14U, 17U, 19U, 21U, 25U, 17U, 14U);
    case '1':
        return TERM_GLYPH7(4U, 12U, 4U, 4U, 4U, 4U, 14U);
    case '2':
        return TERM_GLYPH7(14U, 17U, 1U, 2U, 4U, 8U, 31U);
    case '3':
        return TERM_GLYPH7(30U, 1U, 1U, 14U, 1U, 1U, 30U);
    case '4':
        return TERM_GLYPH7(2U, 6U, 10U, 18U, 31U, 2U, 2U);
    case '5':
        return TERM_GLYPH7(31U, 16U, 16U, 30U, 1U, 1U, 30U);
    case '6':
        return TERM_GLYPH7(14U, 16U, 16U, 30U, 17U, 17U, 14U);
    case '7':
        return TERM_GLYPH7(31U, 1U, 2U, 4U, 8U, 8U, 8U);
    case '8':
        return TERM_GLYPH7(14U, 17U, 17U, 14U, 17U, 17U, 14U);
    case '9':
        return TERM_GLYPH7(14U, 17U, 17U, 15U, 1U, 1U, 14U);
    case '-':
        return TERM_GLYPH7(0U, 0U, 0U, 31U, 0U, 0U, 0U);
    case '>':
        return TERM_GLYPH7(16U, 8U, 4U, 2U, 4U, 8U, 16U);
    case '<':
        return TERM_GLYPH7(1U, 2U, 4U, 8U, 4U, 2U, 1U);
    case '$':
        return TERM_GLYPH7(4U, 15U, 20U, 14U, 5U, 30U, 4U);
    case '#':
        return TERM_GLYPH7(10U, 31U, 10U, 10U, 31U, 10U, 10U);
    case '?':
        return TERM_GLYPH7(14U, 17U, 1U, 2U, 4U, 0U, 4U);
    case '!':
        return TERM_GLYPH7(4U, 4U, 4U, 4U, 4U, 0U, 4U);
    case ',':
        return TERM_GLYPH7(0U, 0U, 0U, 0U, 0U, 4U, 8U);
    case ';':
        return TERM_GLYPH7(0U, 4U, 4U, 0U, 0U, 4U, 8U);
    case '*':
        return TERM_GLYPH7(0U, 21U, 14U, 31U, 14U, 21U, 0U);
    case '(':
        return TERM_GLYPH7(2U, 4U, 8U, 8U, 8U, 4U, 2U);
    case ')':
        return TERM_GLYPH7(8U, 4U, 2U, 2U, 2U, 4U, 8U);
    case '[':
        return TERM_GLYPH7(14U, 8U, 8U, 8U, 8U, 8U, 14U);
    case ']':
        return TERM_GLYPH7(14U, 2U, 2U, 2U, 2U, 2U, 14U);
    case '@':
        return TERM_GLYPH7(14U, 17U, 23U, 21U, 23U, 16U, 15U);
    case '%':
        return TERM_GLYPH7(24U, 25U, 2U, 4U, 8U, 19U, 3U);
    case '&':
        return TERM_GLYPH7(12U, 18U, 20U, 8U, 21U, 18U, 13U);
    case '~':
        return TERM_GLYPH7(0U, 0U, 8U, 21U, 2U, 0U, 0U);
    case '\\':
        return TERM_GLYPH7(16U, 16U, 8U, 4U, 2U, 1U, 1U);
    case '"':
        return TERM_GLYPH7(10U, 10U, 10U, 0U, 0U, 0U, 0U);
    case '\'':
        return TERM_GLYPH7(4U, 4U, 8U, 0U, 0U, 0U, 0U);
    case '_':
        return TERM_GLYPH7(0U, 0U, 0U, 0U, 0U, 0U, 31U);
    case '.':
        return TERM_GLYPH7(0U, 0U, 0U, 0U, 0U, 12U, 12U);
    case ':':
        return TERM_GLYPH7(0U, 12U, 12U, 0U, 12U, 12U, 0U);
    case '/':
        return TERM_GLYPH7(1U, 1U, 2U, 4U, 8U, 16U, 16U);
    case '+':
        return TERM_GLYPH7(0U, 4U, 4U, 31U, 4U, 4U, 0U);
    case '=':
        return TERM_GLYPH7(0U, 0U, 31U, 0U, 31U, 0U, 0U);
    case '^':
        return TERM_GLYPH7(4U, 10U, 17U, 0U, 0U, 0U, 0U);
    case '|':
        return TERM_GLYPH7(4U, 4U, 4U, 4U, 4U, 4U, 4U);
    default:
        return 0ULL;
    }
}

static void term_fill_rect(term_app *app, int x, int y, int w, int h, term_u32 color) {
    int left;
    int top;
    int right;
    int bottom;
    int row;

    if (app == (term_app *)0 || app->pixels == (term_u32 *)0 || app->w <= 0 || app->h <= 0 || w <= 0 || h <= 0) {
        return;
    }

    left = term_clampi(x, 0, app->w);
    top = term_clampi(y, 0, app->h);
    right = term_clampi(x + w, 0, app->w);
    bottom = term_clampi(y + h, 0, app->h);
    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        u64 base = (u64)(unsigned int)row * (u64)(unsigned int)app->w;
        int col;

        if (base + (u64)(unsigned int)right > app->pixel_count) {
            return;
        }
        for (col = left; col < right; col++) {
            app->pixels[base + (u64)(unsigned int)col] = color;
        }
    }
}

static void term_stroke_rect(term_app *app, int x, int y, int w, int h, term_u32 color) {
    term_fill_rect(app, x, y, w, 1, color);
    term_fill_rect(app, x, y + h - 1, w, 1, color);
    term_fill_rect(app, x, y, 1, h, color);
    term_fill_rect(app, x + w - 1, y, 1, h, color);
}

static void term_draw_char(term_app *app, int x, int y, char ch, int scale, term_u32 color) {
    u64 mask = term_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }

    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit_index = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit_index)) != 0ULL) {
                term_fill_rect(app, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void term_draw_char_styled(term_app *app, int x, int y, char ch, term_u32 fg, term_u32 bg, term_u8 style) {
    u64 mask = term_glyph_mask(ch);
    int row;

    term_fill_rect(app, x, y, 6, 11, bg);
    if (mask == 0ULL) {
        if ((style & TERM_STYLE_UNDERLINE) != 0U) {
            term_fill_rect(app, x, y + 8, 5, 1, fg);
        }
        return;
    }

    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit_index = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit_index)) != 0ULL) {
                term_fill_rect(app, x + col, y + row, 1, 1, fg);
                if ((style & TERM_STYLE_BOLD) != 0U && col < 4) {
                    term_fill_rect(app, x + col + 1, y + row, 1, 1, fg);
                }
            }
        }
    }

    if ((style & TERM_STYLE_UNDERLINE) != 0U) {
        term_fill_rect(app, x, y + 8, 5, 1, fg);
    }
}

static void term_draw_text_limit(term_app *app, int x, int y, const char *text, int scale, term_u32 color, int max_x) {
    int cursor_x = x;

    if (app == (term_app *)0 || text == (const char *)0 || scale <= 0) {
        return;
    }
    if (max_x <= 0 || max_x > app->w) {
        max_x = app->w;
    }

    while (*text != '\0' && cursor_x + (5 * scale) <= max_x) {
        if (*text != ' ') {
            term_draw_char(app, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

static void term_draw_text(term_app *app, int x, int y, const char *text, int scale, term_u32 color) {
    term_draw_text_limit(app, x, y, text, scale, color, app != (term_app *)0 ? app->w : 0);
}

static void term_draw_control_button(term_app *app, int x, int active, int kind) {
    term_u32 bg =
        (kind == 2) ? TERM_COLOR_CLOSE : (active != 0 ? TERM_COLOR_CONTROL_ACTIVE : TERM_COLOR_CONTROL_INACTIVE);
    term_u32 fg = (kind == 2 || active != 0) ? TERM_COLOR_WHITE : TERM_COLOR_TEXT;
    int cy = TERM_TITLE_H / 2;
    int cx = x + (TERM_CONTROL_W / 2);

    term_fill_rect(app, x, 0, TERM_CONTROL_W, TERM_TITLE_H, bg);
    if (kind == 0) {
        term_fill_rect(app, cx - 6, cy + 4, 12, 1, fg);
    } else if (kind == 1) {
        term_stroke_rect(app, cx - 6, cy - 6, 12, 12, fg);
        term_fill_rect(app, cx - 6, cy - 6, 12, 2, fg);
    } else if (kind == 3) {
        term_stroke_rect(app, cx - 4, cy - 7, 10, 10, fg);
        term_fill_rect(app, cx - 4, cy - 7, 10, 2, fg);
        term_stroke_rect(app, cx - 7, cy - 3, 10, 10, fg);
        term_fill_rect(app, cx - 7, cy - 3, 10, 2, fg);
    } else {
        int i;
        for (i = 0; i < 11; i++) {
            term_fill_rect(app, cx - 5 + i, cy - 5 + i, 1, 1, fg);
            term_fill_rect(app, cx + 5 - i, cy - 5 + i, 1, 1, fg);
        }
    }
}

static term_u32 term_ansi_palette(u64 index) {
    static const term_u32 palette[16] = {0x00000000U, 0x00CD3131U, 0x000DBC79U, 0x00E5E510U, 0x002472C8U, 0x00BC3FBCU,
                                         0x0011A8CDU, 0x00E5E5E5U, 0x00666666U, 0x00F14C4CU, 0x0023D18BU, 0x00F5F543U,
                                         0x003B8EEAU, 0x00D670D6U, 0x0029B8DBU, 0x00FFFFFFU};

    return (index < 16ULL) ? palette[index] : TERM_COLOR_DEFAULT;
}

static term_u32 term_ansi_clamp_255(u64 value) {
    return (term_u32)((value > 255ULL) ? 255ULL : value);
}

static term_u32 term_ansi_color_from_256(u64 index) {
    if (index < 16ULL) {
        return term_ansi_palette(index);
    }
    if (index <= 231ULL) {
        static const term_u32 steps[6] = {0U, 95U, 135U, 175U, 215U, 255U};
        u64 n = index - 16ULL;
        u64 r = n / 36ULL;
        u64 g = (n / 6ULL) % 6ULL;
        u64 b = n % 6ULL;
        return (steps[r] << 16U) | (steps[g] << 8U) | steps[b];
    }
    if (index <= 255ULL) {
        term_u32 gray = (term_u32)(8ULL + ((index - 232ULL) * 10ULL));
        return (gray << 16U) | (gray << 8U) | gray;
    }
    return TERM_COLOR_DEFAULT;
}

static void term_reset_style(term_app *app) {
    if (app == (term_app *)0) {
        return;
    }
    app->current_fg = TERM_COLOR_DEFAULT;
    app->current_bg = TERM_COLOR_BG;
    app->current_style = TERM_STYLE_NONE;
    app->ansi_bold = 0;
    app->ansi_underline = 0;
    app->ansi_inverse = 0;
}

static term_u32 term_effective_fg(const term_app *app) {
    if (app == (const term_app *)0) {
        return TERM_COLOR_DEFAULT;
    }
    return (app->ansi_inverse != 0) ? app->current_bg : app->current_fg;
}

static term_u32 term_effective_bg(const term_app *app) {
    if (app == (const term_app *)0) {
        return TERM_COLOR_BG;
    }
    return (app->ansi_inverse != 0) ? app->current_fg : app->current_bg;
}

static term_u8 term_effective_style(const term_app *app) {
    term_u8 style = TERM_STYLE_NONE;
    if (app == (const term_app *)0) {
        return style;
    }
    if (app->ansi_bold != 0) {
        style |= TERM_STYLE_BOLD;
    }
    if (app->ansi_underline != 0) {
        style |= TERM_STYLE_UNDERLINE;
    }
    style |= app->current_style;
    return style;
}

static void term_clear_cell(term_app *app, u64 row, u64 col) {
    if (app == (term_app *)0 || row >= (u64)TERM_LINES || col >= (u64)TERM_LINE_MAX) {
        return;
    }
    app->lines[row][col] = ' ';
    app->cell_fg[row][col] = term_effective_fg(app);
    app->cell_bg[row][col] = term_effective_bg(app);
    app->cell_style[row][col] = term_effective_style(app);
}

static void term_trim_line(term_app *app, u64 row) {
    i64 col;

    if (app == (term_app *)0 || row >= (u64)TERM_LINES) {
        return;
    }
    app->lines[row][TERM_LINE_MAX - 1U] = '\0';
    for (col = (i64)TERM_LINE_MAX - 2; col >= 0; col--) {
        if (app->lines[row][(u64)col] != ' ' && app->lines[row][(u64)col] != '\0') {
            app->lines[row][(u64)col + 1ULL] = '\0';
            return;
        }
        app->lines[row][(u64)col] = '\0';
    }
}

static void term_clear_line_range(term_app *app, u64 row, u64 start_col, u64 end_col) {
    u64 col;

    if (app == (term_app *)0 || row >= (u64)TERM_LINES) {
        return;
    }
    if (end_col > (u64)TERM_LINE_MAX - 1ULL) {
        end_col = (u64)TERM_LINE_MAX - 1ULL;
    }
    for (col = start_col; col < end_col; col++) {
        term_clear_cell(app, row, col);
    }
    term_trim_line(app, row);
}

static void term_clear(term_app *app) {
    u64 row;
    u64 col;

    if (app == (term_app *)0) {
        return;
    }
    for (row = 0ULL; row < (u64)TERM_LINES; row++) {
        for (col = 0ULL; col < (u64)TERM_LINE_MAX; col++) {
            app->lines[row][col] = '\0';
            app->cell_fg[row][col] = term_effective_fg(app);
            app->cell_bg[row][col] = term_effective_bg(app);
            app->cell_style[row][col] = term_effective_style(app);
        }
    }
    app->line_count = 0ULL;
    app->cursor_row = 0ULL;
    app->cursor_col = 0ULL;
}

static void term_ensure_row(term_app *app, u64 row) {
    u64 i;

    if (app == (term_app *)0) {
        return;
    }
    if (row >= (u64)TERM_LINES) {
        row = (u64)TERM_LINES - 1ULL;
    }
    while (app->line_count <= row) {
        i = app->line_count;
        app->lines[i][0] = '\0';
        app->cell_fg[i][0] = term_effective_fg(app);
        app->cell_bg[i][0] = term_effective_bg(app);
        app->cell_style[i][0] = term_effective_style(app);
        app->line_count++;
    }
}

static void term_shift_lines(term_app *app) {
    u64 i;
    u64 col;

    if (app == (term_app *)0) {
        return;
    }
    for (i = 1ULL; i < (u64)TERM_LINES; i++) {
        ush_copy(app->lines[i - 1ULL], (u64)TERM_LINE_MAX, app->lines[i]);
        for (col = 0ULL; col < (u64)TERM_LINE_MAX; col++) {
            app->cell_fg[i - 1ULL][col] = app->cell_fg[i][col];
            app->cell_bg[i - 1ULL][col] = app->cell_bg[i][col];
            app->cell_style[i - 1ULL][col] = app->cell_style[i][col];
        }
    }
    app->lines[TERM_LINES - 1U][0] = '\0';
    for (col = 0ULL; col < (u64)TERM_LINE_MAX; col++) {
        app->cell_fg[TERM_LINES - 1U][col] = term_effective_fg(app);
        app->cell_bg[TERM_LINES - 1U][col] = term_effective_bg(app);
        app->cell_style[TERM_LINES - 1U][col] = term_effective_style(app);
    }
}

static void term_newline(term_app *app) {
    if (app == (term_app *)0) {
        return;
    }
    term_ensure_row(app, app->cursor_row);
    app->cursor_col = 0ULL;
    if (app->cursor_row + 1ULL >= (u64)TERM_LINES) {
        term_shift_lines(app);
        app->line_count = (u64)TERM_LINES;
    } else {
        app->cursor_row++;
        term_ensure_row(app, app->cursor_row);
    }
}

static void term_append_char(term_app *app, char ch) {
    char *line;
    u64 len;

    if (app == (term_app *)0) {
        return;
    }
    if (ch == '\r') {
        app->cursor_col = 0ULL;
        return;
    }
    if (ch == '\n') {
        term_newline(app);
        return;
    }
    if (ch == '\t') {
        term_append_char(app, ' ');
        term_append_char(app, ' ');
        term_append_char(app, ' ');
        term_append_char(app, ' ');
        return;
    }
    if (ch < ' ' || ch > '~') {
        ch = '?';
    }
    if (app->cursor_col + 1ULL >= (u64)TERM_LINE_MAX) {
        term_newline(app);
    }

    term_ensure_row(app, app->cursor_row);
    line = app->lines[app->cursor_row];
    len = ush_strlen(line);
    while (len < app->cursor_col && len + 1ULL < (u64)TERM_LINE_MAX) {
        line[len] = ' ';
        app->cell_fg[app->cursor_row][len] = term_effective_fg(app);
        app->cell_bg[app->cursor_row][len] = term_effective_bg(app);
        app->cell_style[app->cursor_row][len] = term_effective_style(app);
        len++;
    }
    line[app->cursor_col] = ch;
    app->cell_fg[app->cursor_row][app->cursor_col] = term_effective_fg(app);
    app->cell_bg[app->cursor_row][app->cursor_col] = term_effective_bg(app);
    app->cell_style[app->cursor_row][app->cursor_col] = term_effective_style(app);
    if (line[app->cursor_col + 1ULL] == '\0') {
        line[app->cursor_col + 1ULL] = '\0';
    }
    app->cursor_col++;
}

static u64 term_ansi_parse_params(const char *params, u64 *out_values, u64 max_values) {
    u64 count = 0ULL;
    u64 value = 0ULL;
    int has_digit = 0;
    u64 i;

    if (out_values == (u64 *)0 || max_values == 0ULL) {
        return 0ULL;
    }
    if (params == (const char *)0 || params[0] == '\0') {
        out_values[0] = 0ULL;
        return 1ULL;
    }
    for (i = 0ULL;; i++) {
        char ch = params[i];
        if (ch >= '0' && ch <= '9') {
            has_digit = 1;
            value = (value * 10ULL) + (u64)(ch - '0');
            continue;
        }
        if (ch == '?' && i == 0ULL) {
            continue;
        }
        if (ch == ';' || ch == '\0') {
            if (count < max_values) {
                out_values[count++] = (has_digit != 0) ? value : 0ULL;
            }
            value = 0ULL;
            has_digit = 0;
            if (ch == '\0') {
                break;
            }
        }
    }
    return (count == 0ULL) ? 1ULL : count;
}

static u64 term_ansi_param_or_default(const u64 *params, u64 count, u64 index, u64 default_value) {
    if (params == (const u64 *)0 || index >= count || params[index] == 0ULL) {
        return default_value;
    }
    return params[index];
}

static void term_apply_sgr(term_app *app, const char *params) {
    u64 values[16];
    u64 count = term_ansi_parse_params(params, values, 16ULL);
    u64 i;

    if (app == (term_app *)0) {
        return;
    }
    for (i = 0ULL; i < count; i++) {
        u64 code = values[i];

        if (code == 0ULL) {
            term_reset_style(app);
        } else if (code == 1ULL) {
            app->ansi_bold = 1;
        } else if (code == 4ULL) {
            app->ansi_underline = 1;
        } else if (code == 7ULL) {
            app->ansi_inverse = 1;
        } else if (code == 21ULL || code == 22ULL) {
            app->ansi_bold = 0;
        } else if (code == 24ULL) {
            app->ansi_underline = 0;
        } else if (code == 27ULL) {
            app->ansi_inverse = 0;
        } else if (code == 39ULL) {
            app->current_fg = TERM_COLOR_DEFAULT;
        } else if (code == 49ULL) {
            app->current_bg = TERM_COLOR_BG;
        } else if (code >= 30ULL && code <= 37ULL) {
            u64 idx = code - 30ULL;
            if (app->ansi_bold != 0) {
                idx += 8ULL;
            }
            app->current_fg = term_ansi_palette(idx);
        } else if (code >= 90ULL && code <= 97ULL) {
            app->current_fg = term_ansi_palette((code - 90ULL) + 8ULL);
        } else if (code >= 40ULL && code <= 47ULL) {
            app->current_bg = term_ansi_palette(code - 40ULL);
        } else if (code >= 100ULL && code <= 107ULL) {
            app->current_bg = term_ansi_palette((code - 100ULL) + 8ULL);
        } else if ((code == 38ULL || code == 48ULL) && i + 1ULL < count) {
            u64 mode = values[i + 1ULL];
            term_u32 color;

            if (mode == 5ULL && i + 2ULL < count) {
                color = term_ansi_color_from_256(values[i + 2ULL]);
                if (code == 38ULL) {
                    app->current_fg = color;
                } else {
                    app->current_bg = color;
                }
                i += 2ULL;
            } else if (mode == 2ULL && i + 4ULL < count) {
                term_u32 r = term_ansi_clamp_255(values[i + 2ULL]);
                term_u32 g = term_ansi_clamp_255(values[i + 3ULL]);
                term_u32 b = term_ansi_clamp_255(values[i + 4ULL]);
                color = (r << 16U) | (g << 8U) | b;
                if (code == 38ULL) {
                    app->current_fg = color;
                } else {
                    app->current_bg = color;
                }
                i += 4ULL;
            }
        }
    }
}

static void term_apply_ansi(term_app *app, char final_ch) {
    u64 values[16];
    u64 count;
    int private_mode;

    if (app == (term_app *)0) {
        return;
    }
    app->ansi_buf[app->ansi_len] = '\0';
    count = term_ansi_parse_params(app->ansi_buf, values, 16ULL);
    private_mode = (app->ansi_buf[0] == '?') ? 1 : 0;

    if (final_ch == 'm') {
        term_apply_sgr(app, app->ansi_buf);
    } else if (final_ch == 'J') {
        u64 mode = (count == 0ULL) ? 0ULL : values[0];
        if (mode == 0ULL) {
            u64 row;
            term_clear_line_range(app, app->cursor_row, app->cursor_col, (u64)TERM_LINE_MAX - 1ULL);
            for (row = app->cursor_row + 1ULL; row < (u64)TERM_LINES; row++) {
                term_clear_line_range(app, row, 0ULL, (u64)TERM_LINE_MAX - 1ULL);
            }
        } else if (mode == 1ULL) {
            u64 row;
            for (row = 0ULL; row < app->cursor_row && row < (u64)TERM_LINES; row++) {
                term_clear_line_range(app, row, 0ULL, (u64)TERM_LINE_MAX - 1ULL);
            }
            term_clear_line_range(app, app->cursor_row, 0ULL, app->cursor_col + 1ULL);
        } else if (mode == 2ULL || mode == 3ULL) {
            term_clear(app);
        }
    } else if (final_ch == 'K') {
        u64 mode = (count == 0ULL) ? 0ULL : values[0];
        if (mode == 0ULL) {
            term_clear_line_range(app, app->cursor_row, app->cursor_col, (u64)TERM_LINE_MAX - 1ULL);
        } else if (mode == 1ULL) {
            term_clear_line_range(app, app->cursor_row, 0ULL, app->cursor_col + 1ULL);
        } else {
            term_clear_line_range(app, app->cursor_row, 0ULL, (u64)TERM_LINE_MAX - 1ULL);
        }
    } else if (final_ch == 'H' || final_ch == 'f') {
        u64 row = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        u64 col = term_ansi_param_or_default(values, count, 1ULL, 1ULL);
        app->cursor_row = (row > 0ULL) ? row - 1ULL : 0ULL;
        app->cursor_col = (col > 0ULL) ? col - 1ULL : 0ULL;
        if (app->cursor_row >= (u64)TERM_LINES) {
            app->cursor_row = (u64)TERM_LINES - 1ULL;
        }
        if (app->cursor_col >= (u64)TERM_LINE_MAX - 1ULL) {
            app->cursor_col = (u64)TERM_LINE_MAX - 2ULL;
        }
        term_ensure_row(app, app->cursor_row);
    } else if (final_ch == 'A') {
        u64 n = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_row = (n > app->cursor_row) ? 0ULL : app->cursor_row - n;
    } else if (final_ch == 'B') {
        u64 n = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_row += n;
        if (app->cursor_row >= (u64)TERM_LINES) {
            app->cursor_row = (u64)TERM_LINES - 1ULL;
        }
        term_ensure_row(app, app->cursor_row);
    } else if (final_ch == 'C') {
        u64 n = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_col += n;
        if (app->cursor_col >= (u64)TERM_LINE_MAX - 1ULL) {
            app->cursor_col = (u64)TERM_LINE_MAX - 2ULL;
        }
    } else if (final_ch == 'D') {
        u64 n = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_col = (n > app->cursor_col) ? 0ULL : app->cursor_col - n;
    } else if (final_ch == 'E' || final_ch == 'F') {
        u64 n = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        if (final_ch == 'E') {
            app->cursor_row += n;
            if (app->cursor_row >= (u64)TERM_LINES) {
                app->cursor_row = (u64)TERM_LINES - 1ULL;
            }
        } else {
            app->cursor_row = (n > app->cursor_row) ? 0ULL : app->cursor_row - n;
        }
        app->cursor_col = 0ULL;
        term_ensure_row(app, app->cursor_row);
    } else if (final_ch == 'G') {
        u64 col = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_col = (col > 0ULL) ? col - 1ULL : 0ULL;
        if (app->cursor_col >= (u64)TERM_LINE_MAX - 1ULL) {
            app->cursor_col = (u64)TERM_LINE_MAX - 2ULL;
        }
    } else if (final_ch == 'd') {
        u64 row = term_ansi_param_or_default(values, count, 0ULL, 1ULL);
        app->cursor_row = (row > 0ULL) ? row - 1ULL : 0ULL;
        if (app->cursor_row >= (u64)TERM_LINES) {
            app->cursor_row = (u64)TERM_LINES - 1ULL;
        }
        term_ensure_row(app, app->cursor_row);
    } else if (final_ch == 's') {
        app->saved_row = app->cursor_row;
        app->saved_col = app->cursor_col;
    } else if (final_ch == 'u') {
        app->cursor_row = (app->saved_row < (u64)TERM_LINES) ? app->saved_row : (u64)TERM_LINES - 1ULL;
        app->cursor_col = (app->saved_col < (u64)TERM_LINE_MAX - 1ULL) ? app->saved_col : (u64)TERM_LINE_MAX - 2ULL;
        term_ensure_row(app, app->cursor_row);
    } else if ((final_ch == 'h' || final_ch == 'l') && private_mode != 0) {
        /* Cursor visibility mode is accepted for compatibility; this terminal does not draw a PTY cursor. */
    }
    app->ansi_state = 0;
    app->ansi_len = 0ULL;
}

static void term_append_ansi_char(term_app *app, char ch) {
    if (app == (term_app *)0) {
        return;
    }

    if (app->ansi_state == 1) {
        if (ch == '[') {
            app->ansi_state = 2;
            app->ansi_len = 0ULL;
            return;
        }
        if (ch == '7') {
            app->saved_row = app->cursor_row;
            app->saved_col = app->cursor_col;
            app->ansi_state = 0;
            app->ansi_len = 0ULL;
            return;
        }
        if (ch == '8') {
            app->cursor_row = (app->saved_row < (u64)TERM_LINES) ? app->saved_row : (u64)TERM_LINES - 1ULL;
            app->cursor_col = (app->saved_col < (u64)TERM_LINE_MAX - 1ULL) ? app->saved_col : (u64)TERM_LINE_MAX - 2ULL;
            term_ensure_row(app, app->cursor_row);
            app->ansi_state = 0;
            app->ansi_len = 0ULL;
            return;
        }
        app->ansi_state = 0;
        app->ansi_len = 0ULL;
        return;
    }

    if (app->ansi_state == 2) {
        if ((ch >= '0' && ch <= '9') || ch == ';' || ch == '?' || ch == '=') {
            if (app->ansi_len + 1ULL < (u64)sizeof(app->ansi_buf)) {
                app->ansi_buf[app->ansi_len++] = ch;
            }
            return;
        }
        term_apply_ansi(app, ch);
        return;
    }

    if (ch == 27) {
        app->ansi_state = 1;
        app->ansi_len = 0ULL;
        return;
    }
    term_append_char(app, ch);
}

static void term_append_text(term_app *app, const char *text) {
    u64 i = 0ULL;

    if (app == (term_app *)0 || text == (const char *)0) {
        return;
    }
    while (text[i] != '\0') {
        term_append_ansi_char(app, text[i]);
        i++;
    }
}

static void term_append_line(term_app *app, const char *text) {
    term_append_text(app, text);
    term_append_char(app, '\n');
}

static void term_append_hex(term_app *app, u64 value) {
    char buf[19];
    i64 nibble;
    u64 pos = 0ULL;

    buf[pos++] = '0';
    buf[pos++] = 'X';
    for (nibble = 15; nibble >= 0; nibble--) {
        u64 current = (value >> (u64)(nibble * 4)) & 0xFULL;
        buf[pos++] = (current < 10ULL) ? (char)('0' + current) : (char)('A' + (current - 10ULL));
    }
    buf[pos] = '\0';
    term_append_text(app, buf);
}

static void term_append_prompt_line(term_app *app, const char *line) {
    term_append_text(app, app->cwd);
    term_append_text(app, " > ");
    term_append_line(app, line);
}

static int term_present(term_app *app) {
    cleonos_wm_present_req req;

    if (app == (term_app *)0 || app->window_id == 0ULL || app->pixels == (term_u32 *)0) {
        return 0;
    }
    req.window_id = app->window_id;
    req.pixels_ptr = (u64)(usize)app->pixels;
    req.src_width = (u64)(unsigned int)app->w;
    req.src_height = (u64)(unsigned int)app->h;
    req.src_pitch_bytes = (u64)(unsigned int)app->w * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

static void term_render(term_app *app) {
    int visible_lines;
    int usable_h;
    int line_h = 11;
    u64 start = 0ULL;
    u64 i;
    char prompt[USH_PATH_MAX + TERM_INPUT_MAX + 8U];
    term_u32 title_bg;
    term_u32 title_fg;

    if (app == (term_app *)0 || app->pixels == (term_u32 *)0) {
        return;
    }

    title_bg = (app->focused != 0) ? TERM_COLOR_WIN_BLUE : TERM_COLOR_TITLE_INACTIVE;
    title_fg = (app->focused != 0) ? TERM_COLOR_WHITE : TERM_COLOR_TEXT;
    term_fill_rect(app, 0, 0, app->w, app->h, TERM_COLOR_BG);
    term_fill_rect(app, 0, 0, app->w, TERM_TITLE_H, title_bg);
    term_fill_rect(app, 0, TERM_TITLE_H, app->w, 1, TERM_COLOR_BORDER);
    term_stroke_rect(app, 0, 0, app->w, app->h, TERM_COLOR_BORDER);
    term_draw_text_limit(app, 12, 12, "TERMINAL", 1, title_fg, app->w - (TERM_CONTROL_W * 3) - 8);
    term_draw_control_button(app, app->w - (TERM_CONTROL_W * 3), app->focused, 0);
    term_draw_control_button(app, app->w - (TERM_CONTROL_W * 2), app->focused, app->maximized != 0 ? 3 : 1);
    term_draw_control_button(app, app->w - TERM_CONTROL_W, app->focused, 2);
    term_fill_rect(app, 0, TERM_TITLE_H + 1, app->w, 30, 0x00111111U);
    term_draw_text(app, 12, TERM_TITLE_H + 11, "CLEONOS PSEUDO TTY", 1, TERM_COLOR_PROMPT);
    term_fill_rect(app, 8, TERM_TITLE_H + 31, app->w - 16, 1, 0x00242424U);

    usable_h = app->h - TERM_TITLE_H - 44 - TERM_BOTTOM_H;
    if (usable_h < line_h) {
        usable_h = line_h;
    }
    visible_lines = usable_h / line_h;
    if (visible_lines < 1) {
        visible_lines = 1;
    }
    if (app->line_count > (u64)visible_lines) {
        start = app->line_count - (u64)visible_lines;
    }

    for (i = start; i < app->line_count; i++) {
        int row = (int)(i - start);
        int y = TERM_TITLE_H + 50 + (row * line_h);
        u64 col;
        const char *line = app->lines[i];

        if (y + 8 >= app->h - TERM_BOTTOM_H) {
            break;
        }
        for (col = 0ULL; line[col] != '\0' && col + 1ULL < (u64)TERM_LINE_MAX; col++) {
            int x = 10 + ((int)col * 6);
            term_u32 fg = app->cell_fg[i][col];
            term_u32 bg = app->cell_bg[i][col];
            term_u8 style = app->cell_style[i][col];

            if (x + 5 >= app->w - 10) {
                break;
            }
            term_draw_char_styled(app, x, y, line[col], fg, bg, style);
        }
    }

    term_fill_rect(app, 0, app->h - TERM_BOTTOM_H, app->w, TERM_BOTTOM_H, TERM_COLOR_BAR);
    term_fill_rect(app, 0, app->h - TERM_BOTTOM_H, app->w, 1, 0x00333333U);
    prompt[0] = '\0';
    ush_copy(prompt, (u64)sizeof(prompt), app->cwd);
    term_append_to(prompt, (u64)sizeof(prompt), " > ");
    term_append_to(prompt, (u64)sizeof(prompt), app->input);
    term_append_to(prompt, (u64)sizeof(prompt), "|");
    term_draw_text_limit(app, 10, app->h - 19, prompt, 1, TERM_COLOR_PROMPT, app->w - 10);

    if (app->maximized == 0) {
        term_fill_rect(app, app->w - 14, app->h - 3, 11, 1, TERM_COLOR_MUTED);
        term_fill_rect(app, app->w - 10, app->h - 7, 7, 1, TERM_COLOR_MUTED);
        term_fill_rect(app, app->w - 6, app->h - 11, 3, 1, TERM_COLOR_MUTED);
    }
}

static int term_render_present(term_app *app) {
    term_render(app);
    return term_present(app);
}

static int term_alloc_pixels(term_app *app, int width, int height) {
    u64 count;

    if (app == (term_app *)0 || width <= 0 || height <= 0) {
        return 0;
    }
    count = (u64)(unsigned int)width * (u64)(unsigned int)height;
    if (count == 0ULL || count > TERM_CANVAS_MAX_PIXELS || count > (((u64)-1) / 4ULL)) {
        return 0;
    }
    app->pixels = (term_u32 *)malloc((size_t)(count * 4ULL));
    if (app->pixels == (term_u32 *)0) {
        return 0;
    }
    app->pixel_count = count;
    app->w = width;
    app->h = height;
    ush_zero(app->pixels, count * 4ULL);
    return 1;
}

static void term_release_pixels(term_app *app) {
    if (app == (term_app *)0) {
        return;
    }
    if (app->pixels != (term_u32 *)0) {
        free(app->pixels);
    }
    app->pixels = (term_u32 *)0;
    app->pixel_count = 0ULL;
}

static int term_apply_geometry(term_app *app, int target_x, int target_y, int target_w, int target_h) {
    cleonos_wm_resize_req resize_req;
    cleonos_wm_move_req move_req;
    term_u32 *new_pixels;
    term_u32 *old_pixels;
    u64 count;
    u64 old_count;
    int work_bottom;
    int new_w;
    int new_h;
    int new_x;
    int new_y;
    int old_w;
    int old_h;

    if (app == (term_app *)0 || app->window_id == 0ULL) {
        return 0;
    }

    work_bottom = app->screen_h - TERM_TASKBAR_H;
    if (work_bottom < TERM_TOP_CLAMP_Y + TERM_MIN_H) {
        work_bottom = app->screen_h;
    }
    new_w = term_clampi(target_w, TERM_MIN_W, app->screen_w);
    new_h = term_clampi(target_h, TERM_MIN_H, work_bottom - TERM_TOP_CLAMP_Y);
    new_x = term_clampi(target_x, 0, app->screen_w - new_w);
    new_y = term_clampi(target_y, TERM_TOP_CLAMP_Y, work_bottom - new_h);

    if (new_w != app->w || new_h != app->h) {
        count = (u64)(unsigned int)new_w * (u64)(unsigned int)new_h;
        if (count == 0ULL || count > TERM_CANVAS_MAX_PIXELS || count > (((u64)-1) / 4ULL)) {
            return 0;
        }
        old_pixels = app->pixels;
        old_count = app->pixel_count;
        old_w = app->w;
        old_h = app->h;
        app->pixels = (term_u32 *)0;
        app->pixel_count = 0ULL;
        free(old_pixels);
        new_pixels = (term_u32 *)malloc((size_t)(count * 4ULL));
        if (new_pixels == (term_u32 *)0) {
            app->pixels = (term_u32 *)malloc((size_t)(old_count * 4ULL));
            app->pixel_count = (app->pixels != (term_u32 *)0) ? old_count : 0ULL;
            app->w = old_w;
            app->h = old_h;
            if (app->pixels != (term_u32 *)0) {
                (void)term_render_present(app);
            }
            return 0;
        }
        ush_zero(new_pixels, count * 4ULL);
        resize_req.window_id = app->window_id;
        resize_req.width = (u64)(unsigned int)new_w;
        resize_req.height = (u64)(unsigned int)new_h;
        if (cleonos_sys_wm_resize(&resize_req) == 0ULL) {
            free(new_pixels);
            app->pixels = (term_u32 *)malloc((size_t)(old_count * 4ULL));
            app->pixel_count = (app->pixels != (term_u32 *)0) ? old_count : 0ULL;
            app->w = old_w;
            app->h = old_h;
            if (app->pixels != (term_u32 *)0) {
                (void)term_render_present(app);
            }
            return 0;
        }
        app->pixels = new_pixels;
        app->pixel_count = count;
        app->w = new_w;
        app->h = new_h;
    }

    if (new_x != app->x || new_y != app->y) {
        move_req.window_id = app->window_id;
        move_req.x = (u64)(i64)new_x;
        move_req.y = (u64)(i64)new_y;
        if (cleonos_sys_wm_move(&move_req) == 0ULL) {
            return 0;
        }
        app->x = new_x;
        app->y = new_y;
    }
    (void)term_render_present(app);
    return 1;
}

static void term_toggle_maximize(term_app *app) {
    int work_h;

    if (app == (term_app *)0 || app->window_id == 0ULL) {
        return;
    }
    work_h = app->screen_h - TERM_TASKBAR_H - TERM_TOP_CLAMP_Y;
    if (work_h < TERM_MIN_H) {
        work_h = app->screen_h - TERM_TOP_CLAMP_Y;
    }
    if (app->maximized == 0) {
        app->restore_x = app->x;
        app->restore_y = app->y;
        app->restore_w = app->w;
        app->restore_h = app->h;
        app->maximized = 1;
        if (term_apply_geometry(app, 0, TERM_TOP_CLAMP_Y, app->screen_w, work_h) == 0) {
            app->maximized = 0;
            (void)term_render_present(app);
        }
        return;
    }
    app->maximized = 0;
    if (term_apply_geometry(app, app->restore_x, app->restore_y, app->restore_w, app->restore_h) == 0) {
        app->maximized = 1;
        (void)term_render_present(app);
    }
}

static const char *term_alias(const char *cmd) {
    if (cmd == (const char *)0) {
        return (const char *)0;
    }
    if (ush_streq(cmd, "dir") != 0) {
        return "ls";
    }
    if (ush_streq(cmd, "cls") != 0) {
        return "clear";
    }
    if (ush_streq(cmd, "poweroff") != 0) {
        return "shutdown";
    }
    if (ush_streq(cmd, "reboot") != 0) {
        return "restart";
    }
    return cmd;
}

static int term_command_ctx_write(const char *cmd, const char *arg, const char *cwd) {
    ush_cmd_ctx ctx;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_copy(ctx.cmd, (u64)sizeof(ctx.cmd), cmd);
    ush_copy(ctx.arg, (u64)sizeof(ctx.arg), arg);
    ush_copy(ctx.cwd, (u64)sizeof(ctx.cwd), cwd);
    return (cleonos_sys_fs_write(USH_CMD_CTX_PATH, (const char *)&ctx, (u64)sizeof(ctx)) != 0ULL) ? 1 : 0;
}

static int term_command_ret_read(ush_cmd_ret *out_ret) {
    u64 got;

    if (out_ret == (ush_cmd_ret *)0) {
        return 0;
    }
    ush_zero(out_ret, (u64)sizeof(*out_ret));
    got = cleonos_sys_fs_read(USH_CMD_RET_PATH, (char *)out_ret, (u64)sizeof(*out_ret));
    return (got == (u64)sizeof(*out_ret)) ? 1 : 0;
}

static void term_apply_ret(term_app *app, const ush_cmd_ret *ret) {
    if (app == (term_app *)0 || ret == (const ush_cmd_ret *)0) {
        return;
    }
    if ((ret->flags & USH_CMD_RET_FLAG_CWD) != 0ULL && ret->cwd[0] == '/') {
        ush_copy(app->cwd, (u64)sizeof(app->cwd), ret->cwd);
    }
    if ((ret->flags & USH_CMD_RET_FLAG_EXIT) != 0ULL) {
        app->running = 0;
    }
}

static void term_drain_fd(term_app *app, u64 fd) {
    char buf[192];
    u64 guard = 0ULL;

    while (guard < 128ULL) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));

        if (got == 0ULL || got == (u64)-1) {
            break;
        }
        if (got < (u64)sizeof(buf)) {
            buf[got] = '\0';
            term_append_text(app, buf);
        } else {
            u64 i;
            for (i = 0ULL; i < got; i++) {
                term_append_ansi_char(app, buf[i]);
            }
        }
        guard++;
    }
}

static void term_emit_status(term_app *app, u64 status) {
    if (status == 0ULL) {
        return;
    }
    if ((status & (1ULL << 63)) != 0ULL) {
        term_append_text(app, "PROCESS TERMINATED: ");
        term_append_hex(app, status);
        term_append_char(app, '\n');
        return;
    }
    term_append_text(app, "EXIT STATUS: ");
    term_append_hex(app, status);
    term_append_char(app, '\n');
}

static void term_exec_external(term_app *app, const char *cmd, const char *arg) {
    ush_state sh;
    ush_cmd_ret ret;
    const char *canonical;
    char path[USH_PATH_MAX];
    char env_line[USH_PATH_MAX + USH_CMD_MAX + 96ULL];
    u64 pty_fd;
    u64 stdin_fd;
    u64 status;

    ush_init_state(&sh);
    ush_copy(sh.cwd, (u64)sizeof(sh.cwd), app->cwd);
    canonical = term_alias(cmd);
    if (canonical == (const char *)0 || ush_resolve_exec_path(&sh, canonical, path, (u64)sizeof(path)) == 0 ||
        cleonos_sys_fs_stat_type(path) != 1ULL) {
        term_append_text(app, "COMMAND NOT FOUND: ");
        term_append_line(app, cmd);
        return;
    }

    pty_fd = cleonos_sys_pty_open();
    if (pty_fd == (u64)-1) {
        term_append_line(app, "PTY OPEN FAILED");
        return;
    }
    stdin_fd = cleonos_sys_fd_open("/dev/null", CLEONOS_O_RDONLY, 0ULL);
    (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);

    if (term_command_ctx_write(canonical, arg, app->cwd) == 0) {
        term_append_line(app, "COMMAND CONTEXT WRITE FAILED");
        if (stdin_fd != (u64)-1) {
            (void)cleonos_sys_fd_close(stdin_fd);
        }
        (void)cleonos_sys_fd_close(pty_fd);
        return;
    }

    env_line[0] = '\0';
    term_append_to(env_line, (u64)sizeof(env_line), "PWD=");
    term_append_to(env_line, (u64)sizeof(env_line), app->cwd);
    term_append_to(env_line, (u64)sizeof(env_line), ";CMD=");
    term_append_to(env_line, (u64)sizeof(env_line), canonical);
    term_append_to(env_line, (u64)sizeof(env_line), ";LAUNCHER=/shell/uwm/terminal.elf");

    (void)term_render_present(app);
    status = cleonos_sys_exec_pathv_io(path, arg, env_line, (stdin_fd == (u64)-1) ? CLEONOS_FD_INHERIT : stdin_fd,
                                       pty_fd, pty_fd);
    term_drain_fd(app, pty_fd);

    if (stdin_fd != (u64)-1) {
        (void)cleonos_sys_fd_close(stdin_fd);
    }
    (void)cleonos_sys_fd_close(pty_fd);
    if (status == (u64)-1) {
        term_append_line(app, "EXEC REQUEST FAILED");
    } else {
        if (term_command_ret_read(&ret) != 0) {
            term_apply_ret(app, &ret);
        }
        term_emit_status(app, status);
    }
    (void)cleonos_sys_fs_remove(USH_CMD_CTX_PATH);
    (void)cleonos_sys_fs_remove(USH_CMD_RET_PATH);
}

static void term_exec_line(term_app *app, const char *line) {
    ush_state sh;
    char local[USH_LINE_MAX];
    char cmd[USH_CMD_MAX];
    char arg[USH_ARG_MAX];
    char path[USH_PATH_MAX];
    const char *canonical;

    if (app == (term_app *)0 || line == (const char *)0) {
        return;
    }
    ush_copy(local, (u64)sizeof(local), line);
    ush_trim_line(local);
    if (local[0] == '\0') {
        term_append_prompt_line(app, "");
        return;
    }

    ush_parse_line(local, cmd, (u64)sizeof(cmd), arg, (u64)sizeof(arg));
    canonical = term_alias(cmd);
    if (canonical != (const char *)0 && ush_streq(canonical, "clear") != 0) {
        term_clear(app);
        return;
    }

    term_append_prompt_line(app, local);
    if (canonical != (const char *)0 && ush_streq(canonical, "help") != 0) {
        term_append_line(app, "BUILTINS: HELP CLEAR PWD CD EXIT");
        term_append_line(app, "EXTERNAL COMMANDS RUN THROUGH PTY OUTPUT CAPTURE");
        return;
    }
    if (canonical != (const char *)0 && ush_streq(canonical, "pwd") != 0) {
        term_append_line(app, app->cwd);
        return;
    }
    if (canonical != (const char *)0 && ush_streq(canonical, "cd") != 0) {
        ush_init_state(&sh);
        ush_copy(sh.cwd, (u64)sizeof(sh.cwd), app->cwd);
        if (ush_resolve_path(&sh, (arg[0] == '\0') ? "/" : arg, path, (u64)sizeof(path)) == 0) {
            term_append_line(app, "CD: INVALID PATH");
            return;
        }
        if (cleonos_sys_fs_stat_type(path) != 2ULL) {
            term_append_line(app, "CD: DIRECTORY NOT FOUND");
            return;
        }
        ush_copy(app->cwd, (u64)sizeof(app->cwd), path);
        return;
    }
    if (canonical != (const char *)0 && ush_streq(canonical, "exit") != 0) {
        app->running = 0;
        return;
    }
    term_exec_external(app, cmd, arg);
}

static void term_handle_key(term_app *app, u64 key) {
    if (app == (term_app *)0) {
        return;
    }
    if (key == 8ULL || key == 127ULL) {
        if (app->input_len > 0ULL) {
            app->input_len--;
            app->input[app->input_len] = '\0';
        }
        return;
    }
    if (key == (u64)'\n' || key == (u64)'\r') {
        char line[TERM_INPUT_MAX];

        ush_copy(line, (u64)sizeof(line), app->input);
        app->input[0] = '\0';
        app->input_len = 0ULL;
        term_exec_line(app, line);
        return;
    }
    if (key >= 32ULL && key <= 126ULL && app->input_len + 1ULL < (u64)sizeof(app->input)) {
        app->input[app->input_len++] = (char)key;
        app->input[app->input_len] = '\0';
    }
}

static int term_hit_resize(const term_app *app, int x, int y) {
    return (app != (const term_app *)0 && x >= app->w - TERM_RESIZE_GRIP && y >= app->h - TERM_RESIZE_GRIP) ? 1 : 0;
}

static void term_handle_mouse_button(term_app *app, const cleonos_wm_event *event) {
    cleonos_mouse_state mouse;
    int local_x;
    int local_y;
    int left_changed;
    int left_down;

    if (app == (term_app *)0 || event == (const cleonos_wm_event *)0) {
        return;
    }
    local_x = term_u64_as_i32(event->arg2);
    local_y = term_u64_as_i32(event->arg3);
    left_changed = ((event->arg1 & 0x1ULL) != 0ULL) ? 1 : 0;
    left_down = ((event->arg0 & 0x1ULL) != 0ULL) ? 1 : 0;
    if (left_changed == 0) {
        return;
    }
    if (left_down == 0) {
        app->dragging = 0;
        app->resizing = 0;
        return;
    }

    ush_zero(&mouse, (u64)sizeof(mouse));
    if (cleonos_sys_mouse_state(&mouse) != 0ULL && mouse.ready != 0ULL) {
        local_x = term_u64_as_i32(mouse.x) - app->x;
        local_y = term_u64_as_i32(mouse.y) - app->y;
    }

    if (local_y >= 0 && local_y < TERM_TITLE_H) {
        if (local_x >= app->w - TERM_CONTROL_W) {
            app->running = 0;
            return;
        }
        if (local_x >= app->w - (TERM_CONTROL_W * 2) && local_x < app->w - TERM_CONTROL_W) {
            term_toggle_maximize(app);
            return;
        }
        if (local_x >= app->w - (TERM_CONTROL_W * 3) && local_x < app->w - (TERM_CONTROL_W * 2)) {
            return;
        }
        if (app->maximized == 0) {
            app->dragging = 1;
            app->drag_dx = local_x;
            app->drag_dy = local_y;
        }
        return;
    }

    if (app->maximized == 0 && term_hit_resize(app, local_x, local_y) != 0) {
        app->resizing = 1;
        app->resize_start_x = app->x + local_x;
        app->resize_start_y = app->y + local_y;
        app->resize_start_w = app->w;
        app->resize_start_h = app->h;
    }
}

static void term_handle_mouse_move(term_app *app, const cleonos_wm_event *event) {
    int global_x;
    int global_y;

    if (app == (term_app *)0 || event == (const cleonos_wm_event *)0) {
        return;
    }
    global_x = term_u64_as_i32(event->arg0);
    global_y = term_u64_as_i32(event->arg1);
    if (app->dragging != 0 && app->maximized == 0) {
        (void)term_apply_geometry(app, global_x - app->drag_dx, global_y - app->drag_dy, app->w, app->h);
        return;
    }
    if (app->resizing != 0 && app->maximized == 0) {
        int next_w = app->resize_start_w + (global_x - app->resize_start_x);
        int next_h = app->resize_start_h + (global_y - app->resize_start_y);
        (void)term_apply_geometry(app, app->x, app->y, next_w, next_h);
    }
}

static void term_loop(term_app *app) {
    int idle_spins = 0;

    while (app->running != 0) {
        int dirty = 0;
        int handled = 0;
        u64 budget = 0ULL;

        while (budget < TERM_EVENT_BUDGET) {
            cleonos_wm_event event;

            ush_zero(&event, (u64)sizeof(event));
            if (cleonos_sys_wm_poll_event(app->window_id, &event) == 0ULL) {
                break;
            }
            handled = 1;
            dirty = 1;
            if (event.type == CLEONOS_WM_EVENT_FOCUS_GAINED) {
                app->focused = 1;
            } else if (event.type == CLEONOS_WM_EVENT_FOCUS_LOST) {
                app->focused = 0;
                app->dragging = 0;
                app->resizing = 0;
            } else if (event.type == CLEONOS_WM_EVENT_KEY) {
                term_handle_key(app, event.arg0);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_BUTTON) {
                term_handle_mouse_button(app, &event);
            } else if (event.type == CLEONOS_WM_EVENT_MOUSE_MOVE) {
                term_handle_mouse_move(app, &event);
                dirty = 0;
            }
            if (app->running == 0) {
                break;
            }
            budget++;
        }

        if (dirty != 0 && app->running != 0) {
            (void)term_render_present(app);
        }
        if (handled != 0 || app->dragging != 0 || app->resizing != 0) {
            idle_spins = 0;
            (void)cleonos_sys_yield();
            continue;
        }
        if (idle_spins < TERM_IDLE_SPINS) {
            idle_spins++;
            (void)cleonos_sys_yield();
            continue;
        }
        idle_spins = 0;
        (void)cleonos_sys_sleep_ticks(1ULL);
    }
}

static int term_load_screen_info(term_app *app) {
    cleonos_fb_info fb;

    if (app == (term_app *)0) {
        return 0;
    }

    ush_zero(&fb, (u64)sizeof(fb));
    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL ||
        fb.width > 4096ULL || fb.height > 4096ULL) {
        return 0;
    }
    app->screen_w = (int)fb.width;
    app->screen_h = (int)fb.height;
    return 1;
}

static void term_set_geometry(term_app *app, int wanted_w, int wanted_h) {
    int max_w;
    int max_h;

    if (app == (term_app *)0) {
        return;
    }
    max_w = app->screen_w - 96;
    max_h = app->screen_h - 128;
    if (max_w < TERM_MIN_W) {
        max_w = app->screen_w;
    }
    if (max_h < TERM_MIN_H) {
        max_h = app->screen_h;
    }
    app->w = term_clampi(wanted_w, TERM_MIN_W, max_w);
    app->h = term_clampi(wanted_h, TERM_MIN_H, max_h);
    app->x = (app->screen_w > app->w) ? ((app->screen_w - app->w) / 2) : 0;
    app->y = (app->screen_h > app->h) ? ((app->screen_h - app->h) / 2) : TERM_TOP_CLAMP_Y;
    if (app->y < TERM_TOP_CLAMP_Y) {
        app->y = TERM_TOP_CLAMP_Y;
    }
}

static int term_init_window(term_app *app) {
    static const int fallback_sizes[][2] = {
        {TERM_DEFAULT_W, TERM_DEFAULT_H},
        {640, 360},
        {480, 300},
    };
    cleonos_wm_create_req req;
    u64 i;

    if (term_load_screen_info(app) == 0) {
        return 0;
    }

    app->old_tty = cleonos_sys_tty_active();
    if (app->old_tty != TERM_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(TERM_TTY_DISPLAY);
        app->tty_switched = 1;
    }

    for (i = 0ULL; i < (u64)(sizeof(fallback_sizes) / sizeof(fallback_sizes[0])); i++) {
        term_release_pixels(app);
        term_set_geometry(app, fallback_sizes[i][0], fallback_sizes[i][1]);
        if (term_alloc_pixels(app, app->w, app->h) == 0) {
            continue;
        }

        req.x = (u64)(i64)app->x;
        req.y = (u64)(i64)app->y;
        req.width = (u64)(unsigned int)app->w;
        req.height = (u64)(unsigned int)app->h;
        req.flags = 0ULL;
        app->window_id = cleonos_sys_wm_create(&req);
        if (app->window_id != 0ULL) {
            (void)cleonos_sys_wm_set_focus(app->window_id);
            app->focused = 1;
            return 1;
        }
    }

    term_release_pixels(app);
    return 0;
}

static void term_shutdown(term_app *app) {
    if (app == (term_app *)0) {
        return;
    }
    if (app->window_id != 0ULL) {
        (void)cleonos_sys_wm_destroy(app->window_id);
        app->window_id = 0ULL;
    }
    if (app->pixels != (term_u32 *)0) {
        term_release_pixels(app);
    }
    if (app->tty_switched != 0) {
        (void)cleonos_sys_tty_switch(app->old_tty);
        app->tty_switched = 0;
    }
}

int cleonos_terminal_run(void) {
    term_app app;

    ush_zero(&app, (u64)sizeof(app));
    app.running = 1;
    term_reset_style(&app);
    ush_copy(app.cwd, (u64)sizeof(app.cwd), "/");
    term_clear(&app);
    term_append_line(&app, "CLEONOS TERMINAL READY");
    term_append_line(&app, "TYPE HELP, PWD, LS, FASTFETCH, IFCONFIG...");

    if (term_init_window(&app) == 0) {
        term_shutdown(&app);
        ush_writeln("terminal: wm init failed");
        return 0;
    }
    (void)term_render_present(&app);
    term_loop(&app);
    term_shutdown(&app);
    return 1;
}
