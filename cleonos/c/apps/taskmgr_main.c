#include <cleonos_syscall.h>

typedef long long i64;
typedef unsigned int tm_u32;

#define TM_TTY_DISPLAY 1ULL
#define TM_CANVAS_MAX_W 800U
#define TM_CANVAS_MAX_H 540U
#define TM_DEFAULT_W 760
#define TM_DEFAULT_H 500
#define TM_MIN_W 520
#define TM_MIN_H 340
#define TM_TITLE_H 32
#define TM_TOOLBAR_H 50
#define TM_HEADER_H 26
#define TM_STATUS_H 28
#define TM_ROW_H 24
#define TM_CLOSE_W 46
#define TM_MAX_ROWS 96U
#define TM_EVENT_BUDGET 96ULL
#define TM_REFRESH_TICKS 45ULL

#define TM_KEY_LEFT 1ULL
#define TM_KEY_RIGHT 2ULL
#define TM_KEY_UP 3ULL
#define TM_KEY_DOWN 4ULL

#define TM_COLOR_WHITE 0x00FFFFFFU
#define TM_COLOR_BG 0x00F3F3F3U
#define TM_COLOR_PANEL 0x00FFFFFFU
#define TM_COLOR_TITLE 0x000078D7U
#define TM_COLOR_CLOSE 0x00E81123U
#define TM_COLOR_TEXT 0x00232323U
#define TM_COLOR_MUTED 0x00666666U
#define TM_COLOR_BORDER 0x00D0D0D0U
#define TM_COLOR_ROW_ALT 0x00FAFAFAU
#define TM_COLOR_SELECT 0x00CDE8FFU
#define TM_COLOR_SELECT_BORDER 0x0078BDE8U
#define TM_COLOR_BUTTON 0x00E7E7E7U
#define TM_COLOR_BUTTON_HOT 0x00D8EBFAU
#define TM_COLOR_WARN 0x00FFF4CEU
#define TM_COLOR_BAD 0x00FDE7E9U
#define TM_COLOR_GOOD 0x00DFF6DDU

#define TM_GLYPH7(r0, r1, r2, r3, r4, r5, r6)                                                                          \
    (((u64)(r0) << 30U) | ((u64)(r1) << 25U) | ((u64)(r2) << 20U) | ((u64)(r3) << 15U) | ((u64)(r4) << 10U) |          \
     ((u64)(r5) << 5U) | (u64)(r6))

typedef struct tm_app {
    int screen_w;
    int screen_h;
    int x;
    int y;
    int w;
    int h;
    u64 window_id;
    u64 old_tty;
    int tty_switched;
    int running;
    int dragging;
    int drag_dx;
    int drag_dy;
    int include_exited;
    int selected;
    int scroll;
    u64 selected_pid;
    u64 total_mem;
    u64 proc_count_raw;
    u64 last_refresh_tick;
    cleonos_proc_snapshot rows[TM_MAX_ROWS];
    u64 row_count;
    char status[160];
} tm_app;

static tm_u32 tm_canvas[TM_CANVAS_MAX_H][TM_CANVAS_MAX_W];

static void tm_zero(void *ptr, u64 size) {
    unsigned char *out = (unsigned char *)ptr;
    u64 i;

    if (ptr == (void *)0) {
        return;
    }

    for (i = 0ULL; i < size; i++) {
        out[i] = 0U;
    }
}

static u64 tm_strlen(const char *text) {
    u64 len = 0ULL;

    if (text == (const char *)0) {
        return 0ULL;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static void tm_copy(char *dst, u64 dst_size, const char *src) {
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL) {
        return;
    }

    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1ULL < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void tm_append(char *dst, u64 dst_size, const char *src) {
    u64 pos;
    u64 i = 0ULL;

    if (dst == (char *)0 || dst_size == 0ULL || src == (const char *)0) {
        return;
    }

    pos = tm_strlen(dst);
    while (src[i] != '\0' && pos + 1ULL < dst_size) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static void tm_append_u64_dec(char *dst, u64 dst_size, u64 value) {
    char tmp[24];
    u64 len = 0ULL;

    if (value == 0ULL) {
        tm_append(dst, dst_size, "0");
        return;
    }

    while (value != 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        char one[2];
        one[0] = tmp[--len];
        one[1] = '\0';
        tm_append(dst, dst_size, one);
    }
}

static void tm_u64_to_dec(char *out, u64 out_size, u64 value) {
    if (out == (char *)0 || out_size == 0ULL) {
        return;
    }
    out[0] = '\0';
    tm_append_u64_dec(out, out_size, value);
}

static int tm_clampi(int value, int min_value, int max_value) {
    if (max_value < min_value) {
        return min_value;
    }
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int tm_u64_as_i32(u64 raw) {
    return (int)(i64)raw;
}

static char tm_upper_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static const char *tm_state_name(u64 state) {
    if (state == CLEONOS_PROC_STATE_PENDING) {
        return "PENDING";
    }
    if (state == CLEONOS_PROC_STATE_RUNNING) {
        return "RUNNING";
    }
    if (state == CLEONOS_PROC_STATE_STOPPED) {
        return "STOPPED";
    }
    if (state == CLEONOS_PROC_STATE_EXITED) {
        return "EXITED";
    }
    return "UNKNOWN";
}

static u64 tm_glyph_mask(char ch) {
    switch (tm_upper_char(ch)) {
    case 'A':
        return TM_GLYPH7(14U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'B':
        return TM_GLYPH7(30U, 17U, 17U, 30U, 17U, 17U, 30U);
    case 'C':
        return TM_GLYPH7(14U, 17U, 16U, 16U, 16U, 17U, 14U);
    case 'D':
        return TM_GLYPH7(30U, 17U, 17U, 17U, 17U, 17U, 30U);
    case 'E':
        return TM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 31U);
    case 'F':
        return TM_GLYPH7(31U, 16U, 16U, 30U, 16U, 16U, 16U);
    case 'G':
        return TM_GLYPH7(14U, 17U, 16U, 23U, 17U, 17U, 15U);
    case 'H':
        return TM_GLYPH7(17U, 17U, 17U, 31U, 17U, 17U, 17U);
    case 'I':
        return TM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 31U);
    case 'J':
        return TM_GLYPH7(1U, 1U, 1U, 1U, 17U, 17U, 14U);
    case 'K':
        return TM_GLYPH7(17U, 18U, 20U, 24U, 20U, 18U, 17U);
    case 'L':
        return TM_GLYPH7(16U, 16U, 16U, 16U, 16U, 16U, 31U);
    case 'M':
        return TM_GLYPH7(17U, 27U, 21U, 21U, 17U, 17U, 17U);
    case 'N':
        return TM_GLYPH7(17U, 25U, 21U, 19U, 17U, 17U, 17U);
    case 'O':
        return TM_GLYPH7(14U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'P':
        return TM_GLYPH7(30U, 17U, 17U, 30U, 16U, 16U, 16U);
    case 'Q':
        return TM_GLYPH7(14U, 17U, 17U, 17U, 21U, 18U, 13U);
    case 'R':
        return TM_GLYPH7(30U, 17U, 17U, 30U, 20U, 18U, 17U);
    case 'S':
        return TM_GLYPH7(15U, 16U, 16U, 14U, 1U, 1U, 30U);
    case 'T':
        return TM_GLYPH7(31U, 4U, 4U, 4U, 4U, 4U, 4U);
    case 'U':
        return TM_GLYPH7(17U, 17U, 17U, 17U, 17U, 17U, 14U);
    case 'V':
        return TM_GLYPH7(17U, 17U, 17U, 17U, 17U, 10U, 4U);
    case 'W':
        return TM_GLYPH7(17U, 17U, 17U, 21U, 21U, 21U, 10U);
    case 'X':
        return TM_GLYPH7(17U, 17U, 10U, 4U, 10U, 17U, 17U);
    case 'Y':
        return TM_GLYPH7(17U, 17U, 10U, 4U, 4U, 4U, 4U);
    case 'Z':
        return TM_GLYPH7(31U, 1U, 2U, 4U, 8U, 16U, 31U);
    case '0':
        return TM_GLYPH7(14U, 17U, 19U, 21U, 25U, 17U, 14U);
    case '1':
        return TM_GLYPH7(4U, 12U, 4U, 4U, 4U, 4U, 14U);
    case '2':
        return TM_GLYPH7(14U, 17U, 1U, 2U, 4U, 8U, 31U);
    case '3':
        return TM_GLYPH7(30U, 1U, 1U, 14U, 1U, 1U, 30U);
    case '4':
        return TM_GLYPH7(2U, 6U, 10U, 18U, 31U, 2U, 2U);
    case '5':
        return TM_GLYPH7(31U, 16U, 16U, 30U, 1U, 1U, 30U);
    case '6':
        return TM_GLYPH7(14U, 16U, 16U, 30U, 17U, 17U, 14U);
    case '7':
        return TM_GLYPH7(31U, 1U, 2U, 4U, 8U, 8U, 8U);
    case '8':
        return TM_GLYPH7(14U, 17U, 17U, 14U, 17U, 17U, 14U);
    case '9':
        return TM_GLYPH7(14U, 17U, 17U, 15U, 1U, 1U, 14U);
    case '-':
        return TM_GLYPH7(0U, 0U, 0U, 31U, 0U, 0U, 0U);
    case '_':
        return TM_GLYPH7(0U, 0U, 0U, 0U, 0U, 0U, 31U);
    case '.':
        return TM_GLYPH7(0U, 0U, 0U, 0U, 0U, 12U, 12U);
    case ':':
        return TM_GLYPH7(0U, 12U, 12U, 0U, 12U, 12U, 0U);
    case '/':
        return TM_GLYPH7(1U, 1U, 2U, 4U, 8U, 16U, 16U);
    case '+':
        return TM_GLYPH7(0U, 4U, 4U, 31U, 4U, 4U, 0U);
    case '=':
        return TM_GLYPH7(0U, 0U, 31U, 0U, 31U, 0U, 0U);
    case '<':
        return TM_GLYPH7(1U, 2U, 4U, 8U, 4U, 2U, 1U);
    case '>':
        return TM_GLYPH7(16U, 8U, 4U, 2U, 4U, 8U, 16U);
    case '[':
        return TM_GLYPH7(14U, 8U, 8U, 8U, 8U, 8U, 14U);
    case ']':
        return TM_GLYPH7(14U, 2U, 2U, 2U, 2U, 2U, 14U);
    case '(':
        return TM_GLYPH7(2U, 4U, 8U, 8U, 8U, 4U, 2U);
    case ')':
        return TM_GLYPH7(8U, 4U, 2U, 2U, 2U, 4U, 8U);
    case '|':
        return TM_GLYPH7(4U, 4U, 4U, 4U, 4U, 4U, 4U);
    case '!':
        return TM_GLYPH7(4U, 4U, 4U, 4U, 4U, 0U, 4U);
    case '?':
        return TM_GLYPH7(14U, 17U, 1U, 2U, 4U, 0U, 4U);
    case '*':
        return TM_GLYPH7(0U, 21U, 14U, 31U, 14U, 21U, 0U);
    default:
        return 0ULL;
    }
}

static void tm_fill_rect(int canvas_w, int canvas_h, int x, int y, int w, int h, tm_u32 color) {
    int left = x;
    int top = y;
    int right = x + w;
    int bottom = y + h;
    int row;

    if (canvas_w <= 0 || canvas_h <= 0 || canvas_w > (int)TM_CANVAS_MAX_W || canvas_h > (int)TM_CANVAS_MAX_H ||
        w <= 0 || h <= 0) {
        return;
    }

    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > canvas_w) {
        right = canvas_w;
    }
    if (bottom > canvas_h) {
        bottom = canvas_h;
    }
    if (left >= right || top >= bottom) {
        return;
    }

    for (row = top; row < bottom; row++) {
        int col;
        for (col = left; col < right; col++) {
            tm_canvas[row][col] = color;
        }
    }
}

static void tm_stroke_rect(int canvas_w, int canvas_h, int x, int y, int w, int h, tm_u32 color) {
    tm_fill_rect(canvas_w, canvas_h, x, y, w, 1, color);
    tm_fill_rect(canvas_w, canvas_h, x, y + h - 1, w, 1, color);
    tm_fill_rect(canvas_w, canvas_h, x, y, 1, h, color);
    tm_fill_rect(canvas_w, canvas_h, x + w - 1, y, 1, h, color);
}

static void tm_draw_char(int canvas_w, int canvas_h, int x, int y, char ch, int scale, tm_u32 color) {
    u64 mask = tm_glyph_mask(ch);
    int row;

    if (mask == 0ULL || scale <= 0) {
        return;
    }

    for (row = 0; row < 7; row++) {
        int col;
        for (col = 0; col < 5; col++) {
            unsigned int bit_index = (unsigned int)((6 - row) * 5 + (4 - col));
            if ((mask & (1ULL << bit_index)) != 0ULL) {
                tm_fill_rect(canvas_w, canvas_h, x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

static void tm_draw_text_limit(int canvas_w, int canvas_h, int x, int y, const char *text, int scale, tm_u32 color,
                               int max_x) {
    int cursor_x = x;

    if (text == (const char *)0 || scale <= 0) {
        return;
    }

    while (*text != '\0' && cursor_x + (5 * scale) <= max_x) {
        if (*text != ' ') {
            tm_draw_char(canvas_w, canvas_h, cursor_x, y, *text, scale, color);
        }
        cursor_x += 6 * scale;
        text++;
    }
}

static void tm_draw_text(int canvas_w, int canvas_h, int x, int y, const char *text, int scale, tm_u32 color) {
    tm_draw_text_limit(canvas_w, canvas_h, x, y, text, scale, color, canvas_w - 4);
}

static void tm_draw_button(int canvas_w, int canvas_h, int x, int y, int w, int h, const char *label, int hot) {
    tm_fill_rect(canvas_w, canvas_h, x, y, w, h, hot != 0 ? TM_COLOR_BUTTON_HOT : TM_COLOR_BUTTON);
    tm_stroke_rect(canvas_w, canvas_h, x, y, w, h, TM_COLOR_BORDER);
    tm_draw_text_limit(canvas_w, canvas_h, x + 10, y + ((h - 7) / 2), label, 1, TM_COLOR_TEXT, x + w - 6);
}

static int tm_visible_rows(const tm_app *app) {
    int list_top;
    int list_bottom;
    int rows;

    if (app == (const tm_app *)0) {
        return 0;
    }

    list_top = TM_TITLE_H + TM_TOOLBAR_H + TM_HEADER_H;
    list_bottom = app->h - TM_STATUS_H;
    rows = (list_bottom - list_top) / TM_ROW_H;
    return (rows > 0) ? rows : 0;
}

static void tm_set_status(tm_app *app, const char *text) {
    if (app != (tm_app *)0) {
        tm_copy(app->status, (u64)sizeof(app->status), text);
    }
}

static void tm_build_summary_status(tm_app *app) {
    if (app == (tm_app *)0) {
        return;
    }

    app->status[0] = '\0';
    tm_append(app->status, (u64)sizeof(app->status), "PROCESSES ");
    tm_append_u64_dec(app->status, (u64)sizeof(app->status), app->row_count);
    tm_append(app->status, (u64)sizeof(app->status), "/");
    tm_append_u64_dec(app->status, (u64)sizeof(app->status), app->proc_count_raw);
    tm_append(app->status, (u64)sizeof(app->status), " | ACTIVE MEM ");
    tm_append_u64_dec(app->status, (u64)sizeof(app->status), app->total_mem / 1024ULL);
    tm_append(app->status, (u64)sizeof(app->status), " KB | R REFRESH A ALL DEL END TASK");
}

static void tm_clamp_selection(tm_app *app) {
    int visible;
    int max_scroll;

    if (app == (tm_app *)0) {
        return;
    }

    if (app->row_count == 0ULL) {
        app->selected = -1;
        app->selected_pid = 0ULL;
        app->scroll = 0;
        return;
    }

    if (app->selected < 0 || (u64)(unsigned int)app->selected >= app->row_count) {
        app->selected = 0;
    }

    app->selected_pid = app->rows[app->selected].pid;
    visible = tm_visible_rows(app);
    if (visible <= 0) {
        app->scroll = 0;
        return;
    }

    max_scroll = ((int)app->row_count > visible) ? ((int)app->row_count - visible) : 0;
    app->scroll = tm_clampi(app->scroll, 0, max_scroll);
    if (app->selected < app->scroll) {
        app->scroll = app->selected;
    }
    if (app->selected >= app->scroll + visible) {
        app->scroll = app->selected - visible + 1;
    }
}

static void tm_reload(tm_app *app) {
    u64 proc_count;
    u64 previous_pid;
    u64 i;
    int previous_index = -1;

    if (app == (tm_app *)0) {
        return;
    }

    previous_pid = app->selected_pid;
    app->row_count = 0ULL;
    app->total_mem = 0ULL;
    proc_count = cleonos_sys_proc_count();
    app->proc_count_raw = proc_count;

    for (i = 0ULL; i < proc_count && app->row_count < (u64)TM_MAX_ROWS; i++) {
        u64 pid = 0ULL;
        cleonos_proc_snapshot snap;

        tm_zero(&snap, (u64)sizeof(snap));
        if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
            continue;
        }
        if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }
        if (app->include_exited == 0 && snap.state == CLEONOS_PROC_STATE_EXITED) {
            continue;
        }
        if (snap.state == CLEONOS_PROC_STATE_RUNNING || snap.state == CLEONOS_PROC_STATE_PENDING ||
            snap.state == CLEONOS_PROC_STATE_STOPPED) {
            app->total_mem += snap.mem_bytes;
        }
        if (snap.pid == previous_pid) {
            previous_index = (int)app->row_count;
        }
        app->rows[app->row_count++] = snap;
    }

    if (previous_index >= 0) {
        app->selected = previous_index;
    } else if (app->row_count != 0ULL) {
        app->selected = 0;
    } else {
        app->selected = -1;
    }

    tm_clamp_selection(app);
    tm_build_summary_status(app);
    app->last_refresh_tick = cleonos_sys_timer_ticks();
}

static int tm_present(const tm_app *app) {
    cleonos_wm_present_req req;

    if (app == (const tm_app *)0 || app->window_id == 0ULL || app->w <= 0 || app->h <= 0) {
        return 0;
    }

    req.window_id = app->window_id;
    req.pixels_ptr = (u64)(usize)&tm_canvas[0][0];
    req.src_width = (u64)(unsigned int)app->w;
    req.src_height = (u64)(unsigned int)app->h;
    req.src_pitch_bytes = (u64)TM_CANVAS_MAX_W * 4ULL;
    return (cleonos_sys_wm_present(&req) != 0ULL) ? 1 : 0;
}

static void tm_draw_titlebar(const tm_app *app) {
    int close_x;

    if (app == (const tm_app *)0) {
        return;
    }

    close_x = app->w - TM_CLOSE_W;
    tm_fill_rect(app->w, app->h, 0, 0, app->w, TM_TITLE_H, TM_COLOR_TITLE);
    tm_draw_text(app->w, app->h, 14, 12, "TASK MANAGER", 1, TM_COLOR_WHITE);
    tm_fill_rect(app->w, app->h, close_x, 0, TM_CLOSE_W, TM_TITLE_H, TM_COLOR_CLOSE);
    tm_draw_text(app->w, app->h, close_x + 18, 12, "X", 1, TM_COLOR_WHITE);
}

static void tm_draw_toolbar(const tm_app *app) {
    int y = TM_TITLE_H;
    int kill_x;

    if (app == (const tm_app *)0) {
        return;
    }

    kill_x = app->w - 122;
    tm_fill_rect(app->w, app->h, 0, y, app->w, TM_TOOLBAR_H, TM_COLOR_PANEL);
    tm_fill_rect(app->w, app->h, 0, y + TM_TOOLBAR_H - 1, app->w, 1, TM_COLOR_BORDER);
    tm_draw_text(app->w, app->h, 18, y + 10, "PROCESSES", 2, TM_COLOR_TEXT);
    tm_draw_button(app->w, app->h, 164, y + 10, 86, 28, "REFRESH", 0);
    tm_draw_button(app->w, app->h, 260, y + 10, 94, 28, app->include_exited != 0 ? "ALL ON" : "ALL OFF", app->include_exited);
    if (kill_x > 370) {
        tm_draw_button(app->w, app->h, kill_x, y + 10, 104, 28, "END TASK", app->selected >= 0);
    }
}

static void tm_draw_header(const tm_app *app) {
    int y = TM_TITLE_H + TM_TOOLBAR_H;

    if (app == (const tm_app *)0) {
        return;
    }

    tm_fill_rect(app->w, app->h, 0, y, app->w, TM_HEADER_H, 0x00EFEFEFU);
    tm_fill_rect(app->w, app->h, 0, y + TM_HEADER_H - 1, app->w, 1, TM_COLOR_BORDER);
    tm_draw_text(app->w, app->h, 18, y + 9, "PID", 1, TM_COLOR_MUTED);
    tm_draw_text(app->w, app->h, 82, y + 9, "STATE", 1, TM_COLOR_MUTED);
    tm_draw_text(app->w, app->h, 172, y + 9, "MEM KB", 1, TM_COLOR_MUTED);
    tm_draw_text(app->w, app->h, 260, y + 9, "TICKS", 1, TM_COLOR_MUTED);
    tm_draw_text(app->w, app->h, 350, y + 9, "IMAGE", 1, TM_COLOR_MUTED);
}

static void tm_draw_row(const tm_app *app, int row_y, int row_index) {
    const cleonos_proc_snapshot *snap;
    char value[32];
    tm_u32 row_bg;
    tm_u32 state_bg;
    int selected;

    if (app == (const tm_app *)0 || row_index < 0 || (u64)(unsigned int)row_index >= app->row_count) {
        return;
    }

    snap = &app->rows[row_index];
    selected = (row_index == app->selected) ? 1 : 0;
    row_bg = selected != 0 ? TM_COLOR_SELECT : ((row_index & 1) != 0 ? TM_COLOR_ROW_ALT : TM_COLOR_PANEL);

    tm_fill_rect(app->w, app->h, 0, row_y, app->w, TM_ROW_H, row_bg);
    if (selected != 0) {
        tm_fill_rect(app->w, app->h, 0, row_y, 4, TM_ROW_H, TM_COLOR_SELECT_BORDER);
    }
    tm_fill_rect(app->w, app->h, 0, row_y + TM_ROW_H - 1, app->w, 1, 0x00E9E9E9U);

    tm_u64_to_dec(value, (u64)sizeof(value), snap->pid);
    tm_draw_text_limit(app->w, app->h, 18, row_y + 8, value, 1, TM_COLOR_TEXT, 78);

    state_bg = TM_COLOR_GOOD;
    if (snap->state == CLEONOS_PROC_STATE_STOPPED) {
        state_bg = TM_COLOR_WARN;
    } else if (snap->state == CLEONOS_PROC_STATE_EXITED) {
        state_bg = TM_COLOR_BAD;
    }
    tm_fill_rect(app->w, app->h, 78, row_y + 4, 78, 16, state_bg);
    tm_stroke_rect(app->w, app->h, 78, row_y + 4, 78, 16, 0x00D9D9D9U);
    tm_draw_text_limit(app->w, app->h, 84, row_y + 9, tm_state_name(snap->state), 1, TM_COLOR_TEXT, 152);

    tm_u64_to_dec(value, (u64)sizeof(value), snap->mem_bytes / 1024ULL);
    tm_draw_text_limit(app->w, app->h, 172, row_y + 8, value, 1, TM_COLOR_TEXT, 250);

    tm_u64_to_dec(value, (u64)sizeof(value), snap->runtime_ticks);
    tm_draw_text_limit(app->w, app->h, 260, row_y + 8, value, 1, TM_COLOR_TEXT, 342);

    tm_draw_text_limit(app->w, app->h, 350, row_y + 8, snap->path, 1, TM_COLOR_TEXT, app->w - 16);
}

static void tm_draw_rows(const tm_app *app) {
    int list_top;
    int visible;
    int i;

    if (app == (const tm_app *)0) {
        return;
    }

    list_top = TM_TITLE_H + TM_TOOLBAR_H + TM_HEADER_H;
    visible = tm_visible_rows(app);
    tm_fill_rect(app->w, app->h, 0, list_top, app->w, app->h - list_top - TM_STATUS_H, TM_COLOR_PANEL);

    if (app->row_count == 0ULL) {
        tm_draw_text(app->w, app->h, 28, list_top + 32, "NO PROCESS SNAPSHOTS", 2, TM_COLOR_MUTED);
        return;
    }

    for (i = 0; i < visible; i++) {
        int row_index = app->scroll + i;
        if ((u64)(unsigned int)row_index >= app->row_count) {
            break;
        }
        tm_draw_row(app, list_top + (i * TM_ROW_H), row_index);
    }

    if (app->scroll > 0) {
        tm_draw_text(app->w, app->h, app->w - 28, list_top + 8, "^", 1, TM_COLOR_MUTED);
    }
    if ((u64)(unsigned int)(app->scroll + visible) < app->row_count) {
        tm_draw_text(app->w, app->h, app->w - 28, app->h - TM_STATUS_H - 18, "V", 1, TM_COLOR_MUTED);
    }
}

static void tm_draw_status(const tm_app *app) {
    int y;

    if (app == (const tm_app *)0) {
        return;
    }

    y = app->h - TM_STATUS_H;
    tm_fill_rect(app->w, app->h, 0, y, app->w, TM_STATUS_H, 0x00F7F7F7U);
    tm_fill_rect(app->w, app->h, 0, y, app->w, 1, TM_COLOR_BORDER);
    tm_draw_text_limit(app->w, app->h, 14, y + 10, app->status, 1, TM_COLOR_MUTED, app->w - 12);
}

static void tm_render(tm_app *app) {
    if (app == (tm_app *)0) {
        return;
    }

    tm_fill_rect(app->w, app->h, 0, 0, app->w, app->h, TM_COLOR_BG);
    tm_draw_titlebar(app);
    tm_draw_toolbar(app);
    tm_draw_header(app);
    tm_draw_rows(app);
    tm_draw_status(app);
    (void)tm_present(app);
}

static void tm_select_row(tm_app *app, int index) {
    if (app == (tm_app *)0) {
        return;
    }

    if (app->row_count == 0ULL) {
        app->selected = -1;
        app->selected_pid = 0ULL;
        return;
    }

    app->selected = tm_clampi(index, 0, (int)app->row_count - 1);
    tm_clamp_selection(app);
}

static void tm_move_selection(tm_app *app, int delta) {
    if (app == (tm_app *)0 || app->row_count == 0ULL) {
        return;
    }
    tm_select_row(app, app->selected + delta);
}

static void tm_kill_selected(tm_app *app) {
    u64 self_pid;
    u64 target_pid;

    if (app == (tm_app *)0 || app->selected < 0 || (u64)(unsigned int)app->selected >= app->row_count) {
        tm_set_status(app, "NO PROCESS SELECTED");
        return;
    }

    self_pid = cleonos_sys_getpid();
    target_pid = app->rows[app->selected].pid;
    if (target_pid == 0ULL) {
        tm_set_status(app, "INVALID PROCESS");
        return;
    }
    if (target_pid == self_pid) {
        tm_set_status(app, "REFUSED TO END TASK MANAGER ITSELF");
        return;
    }

    if (cleonos_sys_proc_kill(target_pid, CLEONOS_SIGTERM) == 0ULL) {
        tm_set_status(app, "END TASK FAILED");
        return;
    }

    tm_reload(app);
    tm_set_status(app, "SIGTERM SENT");
}

static int tm_hit_close(const tm_app *app, int local_x, int local_y) {
    if (app == (const tm_app *)0) {
        return 0;
    }
    return (local_x >= app->w - TM_CLOSE_W && local_x < app->w && local_y >= 0 && local_y < TM_TITLE_H) ? 1 : 0;
}

static int tm_hit_rect(int local_x, int local_y, int x, int y, int w, int h) {
    return (local_x >= x && local_x < x + w && local_y >= y && local_y < y + h) ? 1 : 0;
}

static void tm_handle_toolbar_click(tm_app *app, int local_x, int local_y) {
    int y = TM_TITLE_H;
    int kill_x;

    if (app == (tm_app *)0 || local_y < y || local_y >= y + TM_TOOLBAR_H) {
        return;
    }

    kill_x = app->w - 122;
    if (tm_hit_rect(local_x, local_y, 164, y + 10, 86, 28) != 0) {
        tm_reload(app);
        tm_render(app);
        return;
    }
    if (tm_hit_rect(local_x, local_y, 260, y + 10, 94, 28) != 0) {
        app->include_exited = (app->include_exited == 0) ? 1 : 0;
        tm_reload(app);
        tm_render(app);
        return;
    }
    if (kill_x > 370 && tm_hit_rect(local_x, local_y, kill_x, y + 10, 104, 28) != 0) {
        tm_kill_selected(app);
        tm_render(app);
    }
}

static void tm_handle_list_click(tm_app *app, int local_y) {
    int list_top;
    int list_bottom;
    int row_index;

    if (app == (tm_app *)0) {
        return;
    }

    list_top = TM_TITLE_H + TM_TOOLBAR_H + TM_HEADER_H;
    list_bottom = app->h - TM_STATUS_H;
    if (local_y < list_top || local_y >= list_bottom) {
        return;
    }

    row_index = app->scroll + ((local_y - list_top) / TM_ROW_H);
    if (row_index >= 0 && (u64)(unsigned int)row_index < app->row_count) {
        tm_select_row(app, row_index);
        tm_render(app);
    }
}

static void tm_handle_key(tm_app *app, u64 key) {
    if (app == (tm_app *)0) {
        return;
    }

    if (key == (u64)'q' || key == (u64)'Q' || key == 27ULL) {
        app->running = 0;
        return;
    }
    if (key == (u64)'r' || key == (u64)'R') {
        tm_reload(app);
        tm_render(app);
        return;
    }
    if (key == (u64)'a' || key == (u64)'A') {
        app->include_exited = (app->include_exited == 0) ? 1 : 0;
        tm_reload(app);
        tm_render(app);
        return;
    }
    if (key == (u64)'k' || key == (u64)'K' || key == 127ULL) {
        tm_kill_selected(app);
        tm_render(app);
        return;
    }
    if (key == TM_KEY_UP || key == (u64)'w' || key == (u64)'W') {
        tm_move_selection(app, -1);
        tm_render(app);
        return;
    }
    if (key == TM_KEY_DOWN || key == (u64)'s' || key == (u64)'S') {
        tm_move_selection(app, 1);
        tm_render(app);
        return;
    }
    if (key == TM_KEY_LEFT) {
        tm_move_selection(app, -tm_visible_rows(app));
        tm_render(app);
        return;
    }
    if (key == TM_KEY_RIGHT) {
        tm_move_selection(app, tm_visible_rows(app));
        tm_render(app);
    }
}

static void tm_handle_event(tm_app *app, const cleonos_wm_event *event) {
    if (app == (tm_app *)0 || event == (const cleonos_wm_event *)0) {
        return;
    }

    if (event->type == CLEONOS_WM_EVENT_KEY) {
        tm_handle_key(app, event->arg0);
        return;
    }

    if (event->type == CLEONOS_WM_EVENT_MOUSE_BUTTON) {
        u64 buttons = event->arg0;
        u64 changed = event->arg1;
        int local_x = tm_u64_as_i32(event->arg2);
        int local_y = tm_u64_as_i32(event->arg3);
        int left_changed = ((changed & 0x1ULL) != 0ULL) ? 1 : 0;
        int left_down = ((buttons & 0x1ULL) != 0ULL) ? 1 : 0;

        if (left_changed == 0) {
            return;
        }
        if (left_down == 0) {
            app->dragging = 0;
            return;
        }
        if (tm_hit_close(app, local_x, local_y) != 0) {
            app->running = 0;
            return;
        }
        if (local_y >= 0 && local_y < TM_TITLE_H) {
            app->dragging = 1;
            app->drag_dx = local_x;
            app->drag_dy = local_y;
            return;
        }
        tm_handle_toolbar_click(app, local_x, local_y);
        tm_handle_list_click(app, local_y);
        return;
    }

    if (event->type == CLEONOS_WM_EVENT_MOUSE_MOVE && app->dragging != 0) {
        cleonos_wm_move_req req;
        app->x = tm_u64_as_i32(event->arg0) - app->drag_dx;
        app->y = tm_u64_as_i32(event->arg1) - app->drag_dy;
        req.window_id = app->window_id;
        req.x = (u64)(i64)app->x;
        req.y = (u64)(i64)app->y;
        (void)cleonos_sys_wm_move(&req);
    }
}

static void tm_loop(tm_app *app) {
    if (app == (tm_app *)0) {
        return;
    }

    while (app->running != 0) {
        u64 budget = 0ULL;
        int handled = 0;

        while (budget < TM_EVENT_BUDGET) {
            cleonos_wm_event event;
            tm_zero(&event, (u64)sizeof(event));
            if (cleonos_sys_wm_poll_event(app->window_id, &event) == 0ULL) {
                break;
            }
            tm_handle_event(app, &event);
            handled = 1;
            if (app->running == 0) {
                break;
            }
            budget++;
        }

        if (app->running == 0) {
            break;
        }

        if (cleonos_sys_timer_ticks() - app->last_refresh_tick >= TM_REFRESH_TICKS) {
            tm_reload(app);
            tm_render(app);
            handled = 1;
        }

        if (handled != 0) {
            (void)cleonos_sys_yield();
        } else {
            (void)cleonos_sys_sleep_ticks(1ULL);
        }
    }
}

static int tm_choose_geometry(tm_app *app) {
    cleonos_fb_info fb;
    int max_w;
    int max_h;

    if (app == (tm_app *)0) {
        return 0;
    }

    tm_zero(&fb, (u64)sizeof(fb));
    if (cleonos_sys_fb_info(&fb) == 0ULL || fb.width == 0ULL || fb.height == 0ULL || fb.bpp != 32ULL) {
        return 0;
    }
    if (fb.width > 4096ULL || fb.height > 4096ULL) {
        return 0;
    }

    app->screen_w = (int)fb.width;
    app->screen_h = (int)fb.height;
    max_w = app->screen_w - 80;
    max_h = app->screen_h - 120;
    if (max_w < TM_MIN_W) {
        max_w = app->screen_w;
    }
    if (max_h < TM_MIN_H) {
        max_h = app->screen_h;
    }
    if (max_w > (int)TM_CANVAS_MAX_W) {
        max_w = (int)TM_CANVAS_MAX_W;
    }
    if (max_h > (int)TM_CANVAS_MAX_H) {
        max_h = (int)TM_CANVAS_MAX_H;
    }

    app->w = tm_clampi(TM_DEFAULT_W, TM_MIN_W, max_w);
    app->h = tm_clampi(TM_DEFAULT_H, TM_MIN_H, max_h);
    app->x = (app->screen_w > app->w) ? ((app->screen_w - app->w) / 2) : 0;
    app->y = (app->screen_h > app->h) ? ((app->screen_h - app->h) / 2) : 0;
    return 1;
}

static int tm_create_window(tm_app *app) {
    cleonos_wm_create_req req;

    if (app == (tm_app *)0) {
        return 0;
    }

    req.x = (u64)(i64)app->x;
    req.y = (u64)(i64)app->y;
    req.width = (u64)(unsigned int)app->w;
    req.height = (u64)(unsigned int)app->h;
    req.flags = 0ULL;
    app->window_id = cleonos_sys_wm_create(&req);
    return (app->window_id != 0ULL) ? 1 : 0;
}

static void tm_switch_to_display(tm_app *app) {
    if (app == (tm_app *)0) {
        return;
    }

    app->old_tty = cleonos_sys_tty_active();
    if (app->old_tty != TM_TTY_DISPLAY) {
        (void)cleonos_sys_tty_switch(TM_TTY_DISPLAY);
        app->tty_switched = 1;
    }
}

static void tm_restore_tty(tm_app *app) {
    if (app == (tm_app *)0) {
        return;
    }

    if (app->tty_switched != 0) {
        (void)cleonos_sys_tty_switch(app->old_tty);
        app->tty_switched = 0;
    }
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    tm_app app;

    (void)argc;
    (void)argv;
    (void)envp;

    tm_zero(&app, (u64)sizeof(app));
    app.selected = -1;
    app.running = 1;
    tm_set_status(&app, "STARTING TASK MANAGER");

    if (tm_choose_geometry(&app) == 0) {
        return 1;
    }

    tm_switch_to_display(&app);
    tm_reload(&app);
    if (tm_create_window(&app) == 0) {
        tm_restore_tty(&app);
        return 1;
    }

    tm_render(&app);
    (void)cleonos_sys_wm_set_focus(app.window_id);
    tm_loop(&app);
    (void)cleonos_sys_wm_destroy(app.window_id);
    tm_restore_tty(&app);
    return 0;
}
