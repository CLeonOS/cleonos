#include <termbox2.h>

#include <cleonos_syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tb_state {
    int initialized;
    int width;
    int height;
    int input_mode;
    int output_mode;
    int last_errno;
    int cursor_x;
    int cursor_y;
    uintattr_t clear_fg;
    uintattr_t clear_bg;
    struct tb_cell *back;
    struct tb_cell *front;
} tb_state;

static tb_state tb_global;

static size_t tb_strlen_local(const char *text) {
    size_t len = 0U;
    if (text == (const char *)0) {
        return 0U;
    }
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static void tb_write_raw(const char *text, size_t len) {
    if (text == (const char *)0 || len == 0U) {
        return;
    }
    (void)cleonos_sys_fd_write(1ULL, text, (u64)len);
}

static void tb_write_cstr(const char *text) {
    tb_write_raw(text, tb_strlen_local(text));
}

static int tb_append_dec(char *out, size_t out_size, int value) {
    char tmp[24];
    size_t len = 0U;
    size_t i;
    unsigned int mag;

    if (out == (char *)0 || out_size == 0U) {
        return 0;
    }

    if (value < 0) {
        if (out_size < 2U) {
            out[0] = '\0';
            return 0;
        }
        out[len++] = '-';
        mag = (unsigned int)(-(value + 1)) + 1U;
    } else {
        mag = (unsigned int)value;
    }

    if (mag == 0U) {
        if (len + 1U < out_size) {
            out[len++] = '0';
        }
        out[len] = '\0';
        return (int)len;
    }

    i = 0U;
    while (mag != 0U && i < sizeof(tmp)) {
        tmp[i++] = (char)('0' + (mag % 10U));
        mag /= 10U;
    }

    while (i > 0U && len + 1U < out_size) {
        out[len++] = tmp[--i];
    }

    out[len] = '\0';
    return (int)len;
}

static int tb_query_grid(int *out_w, int *out_h) {
    cleonos_tty_grid_info info;

    if (out_w == (int *)0 || out_h == (int *)0) {
        return TB_ERR;
    }

    memset(&info, 0, sizeof(info));
    if (cleonos_sys_tty_grid_info(&info) == 0ULL || info.cols == 0ULL || info.rows == 0ULL) {
        return TB_ERR;
    }

    *out_w = (int)info.cols;
    *out_h = (int)info.rows;
    return TB_OK;
}

static void tb_free_buffers(void) {
    if (tb_global.back != (struct tb_cell *)0) {
        free(tb_global.back);
        tb_global.back = (struct tb_cell *)0;
    }
    if (tb_global.front != (struct tb_cell *)0) {
        free(tb_global.front);
        tb_global.front = (struct tb_cell *)0;
    }
}

static int tb_alloc_buffers(int w, int h) {
    size_t count;

    if (w <= 0 || h <= 0) {
        return TB_ERR;
    }

    count = (size_t)w * (size_t)h;
    tb_free_buffers();

    tb_global.back = (struct tb_cell *)calloc(count, sizeof(struct tb_cell));
    tb_global.front = (struct tb_cell *)calloc(count, sizeof(struct tb_cell));
    if (tb_global.back == (struct tb_cell *)0 || tb_global.front == (struct tb_cell *)0) {
        tb_free_buffers();
        return TB_ERR_MEM;
    }

    tb_global.width = w;
    tb_global.height = h;
    return TB_OK;
}

static void tb_fill_cell(struct tb_cell *cell, uint32_t ch, uintattr_t fg, uintattr_t bg) {
    cell->ch = ch;
    cell->fg = fg;
    cell->bg = bg;
}

static int tb_color_to_ansi(uintattr_t attr, int background) {
    unsigned int base = (background != 0) ? 40U : 30U;
    unsigned int color = (unsigned int)(attr & 0xFFU);

    if (color == TB_DEFAULT || color == 0U) {
        return (background != 0) ? 49 : 39;
    }

    if (color > TB_WHITE) {
        color = TB_WHITE;
    }

    return (int)(base + (color - 1U));
}

static void tb_emit_move(int x, int y) {
    char seq[32];
    int at = 0;

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    seq[at++] = '\x1B';
    seq[at++] = '[';
    at += tb_append_dec(seq + at, sizeof(seq) - (size_t)at, y + 1);
    seq[at++] = ';';
    at += tb_append_dec(seq + at, sizeof(seq) - (size_t)at, x + 1);
    seq[at++] = 'H';
    seq[at] = '\0';
    tb_write_cstr(seq);
}

static void tb_emit_style(uintattr_t fg, uintattr_t bg) {
    char seq[64];
    int at = 0;

    seq[at++] = '\x1B';
    seq[at++] = '[';
    seq[at++] = '0';
    if ((fg & TB_BOLD) != 0U) {
        seq[at++] = ';';
        seq[at++] = '1';
    }
    if ((fg & TB_UNDERLINE) != 0U) {
        seq[at++] = ';';
        seq[at++] = '4';
    }
    if ((fg & TB_REVERSE) != 0U || (bg & TB_REVERSE) != 0U) {
        seq[at++] = ';';
        seq[at++] = '7';
    }
    seq[at++] = ';';
    at += tb_append_dec(seq + at, sizeof(seq) - (size_t)at, tb_color_to_ansi(fg, 0));
    seq[at++] = ';';
    at += tb_append_dec(seq + at, sizeof(seq) - (size_t)at, tb_color_to_ansi(bg, 1));
    seq[at++] = 'm';
    seq[at] = '\0';
    tb_write_cstr(seq);
}

static int tb_cell_equal(const struct tb_cell *left, const struct tb_cell *right) {
    return left->ch == right->ch && left->fg == right->fg && left->bg == right->bg;
}

static int tb_refresh_size_if_needed(struct tb_event *resize_event) {
    int w;
    int h;

    if (tb_query_grid(&w, &h) != TB_OK) {
        return TB_ERR;
    }

    if (w == tb_global.width && h == tb_global.height) {
        return TB_OK;
    }

    if (tb_alloc_buffers(w, h) != TB_OK) {
        return TB_ERR_MEM;
    }

    if (resize_event != (struct tb_event *)0) {
        memset(resize_event, 0, sizeof(*resize_event));
        resize_event->type = TB_EVENT_RESIZE;
        resize_event->w = w;
        resize_event->h = h;
    }
    return TB_OK;
}

static int tb_map_escape_sequence(const char *seq, size_t len, struct tb_event *event) {
    if (len == 1U && seq[0] == 27) {
        event->type = TB_EVENT_KEY;
        event->key = TB_KEY_ESC;
        return TB_OK;
    }

    if (len == 3U && seq[0] == 27 && seq[1] == '[') {
        event->type = TB_EVENT_KEY;
        switch (seq[2]) {
            case 'A':
                event->key = TB_KEY_ARROW_UP;
                return TB_OK;
            case 'B':
                event->key = TB_KEY_ARROW_DOWN;
                return TB_OK;
            case 'C':
                event->key = TB_KEY_ARROW_RIGHT;
                return TB_OK;
            case 'D':
                event->key = TB_KEY_ARROW_LEFT;
                return TB_OK;
            case 'H':
                event->key = TB_KEY_HOME;
                return TB_OK;
            case 'F':
                event->key = TB_KEY_END;
                return TB_OK;
        }
    }

    if (len == 4U && seq[0] == 27 && seq[1] == '[' && seq[3] == '~') {
        event->type = TB_EVENT_KEY;
        switch (seq[2]) {
            case '2':
                event->key = TB_KEY_INSERT;
                return TB_OK;
            case '3':
                event->key = TB_KEY_DELETE;
                return TB_OK;
            case '5':
                event->key = TB_KEY_PGUP;
                return TB_OK;
            case '6':
                event->key = TB_KEY_PGDN;
                return TB_OK;
        }
    }

    event->type = TB_EVENT_KEY;
    event->key = TB_KEY_ESC;
    return TB_OK;
}

static int tb_read_byte_blocking(char *out_ch, int timeout_ms) {
    u64 got;
    u64 start_ms;

    if (out_ch == (char *)0) {
        return TB_ERR;
    }

    start_ms = cleonos_sys_time_ms();
    for (;;) {
        got = cleonos_sys_fd_read(0ULL, out_ch, 1ULL);
        if (got == 1ULL) {
            return TB_OK;
        }

        if (timeout_ms >= 0) {
            u64 now = cleonos_sys_time_ms();
            if (now >= start_ms && (now - start_ms) >= (u64)timeout_ms) {
                return TB_ERR_NO_EVENT;
            }
        }

        (void)cleonos_sys_yield();
    }
}

int tb_init(void) {
    return tb_init_rwfd(0, 1);
}

int tb_init_file(const char *path) {
    (void)path;
    return tb_init();
}

int tb_init_fd(int ttyfd) {
    (void)ttyfd;
    return tb_init();
}

int tb_init_rwfd(int rfd, int wfd) {
    int w;
    int h;

    (void)rfd;
    (void)wfd;

    if (tb_global.initialized != 0) {
        return TB_ERR_INIT_ALREADY;
    }

    memset(&tb_global, 0, sizeof(tb_global));
    tb_global.cursor_x = -1;
    tb_global.cursor_y = -1;
    tb_global.input_mode = TB_INPUT_ESC;
    tb_global.output_mode = TB_OUTPUT_NORMAL;
    tb_global.clear_fg = TB_DEFAULT;
    tb_global.clear_bg = TB_DEFAULT;

    if (tb_query_grid(&w, &h) != TB_OK) {
        return TB_ERR;
    }

    if (tb_alloc_buffers(w, h) != TB_OK) {
        return TB_ERR_MEM;
    }

    tb_global.initialized = 1;
    tb_write_cstr("\x1B[?25l");
    tb_write_cstr("\x1B[2J\x1B[H");
    return TB_OK;
}

int tb_shutdown(void) {
    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }

    tb_write_cstr("\x1B[0m\x1B[?25h\n");
    tb_free_buffers();
    memset(&tb_global, 0, sizeof(tb_global));
    return TB_OK;
}

int tb_width(void) {
    return (tb_global.initialized != 0) ? tb_global.width : -1;
}

int tb_height(void) {
    return (tb_global.initialized != 0) ? tb_global.height : -1;
}

int tb_clear(void) {
    size_t i;
    size_t count;

    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }

    count = (size_t)tb_global.width * (size_t)tb_global.height;
    for (i = 0U; i < count; i++) {
        tb_fill_cell(&tb_global.back[i], ' ', tb_global.clear_fg, tb_global.clear_bg);
    }
    return TB_OK;
}

int tb_set_clear_attrs(uintattr_t fg, uintattr_t bg) {
    tb_global.clear_fg = fg;
    tb_global.clear_bg = bg;
    return TB_OK;
}

int tb_present(void) {
    int x;
    int y;

    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }

    for (y = 0; y < tb_global.height; y++) {
        for (x = 0; x < tb_global.width; x++) {
            size_t index = (size_t)y * (size_t)tb_global.width + (size_t)x;
            struct tb_cell *back = &tb_global.back[index];
            struct tb_cell *front = &tb_global.front[index];
            char utf8[8];
            int len;

            if (tb_cell_equal(back, front) != 0) {
                continue;
            }

            *front = *back;
            tb_emit_move(x, y);
            tb_emit_style(back->fg, back->bg);
            len = tb_utf8_unicode_to_char(utf8, (back->ch != 0U) ? back->ch : (uint32_t)' ');
            if (len <= 0) {
                utf8[0] = ' ';
                utf8[1] = '\0';
                len = 1;
            }
            tb_write_raw(utf8, (size_t)len);
        }
    }

    if (tb_global.cursor_x >= 0 && tb_global.cursor_y >= 0) {
        tb_write_cstr("\x1B[?25h");
        tb_emit_move(tb_global.cursor_x, tb_global.cursor_y);
    } else {
        tb_write_cstr("\x1B[?25l");
    }

    return TB_OK;
}

int tb_invalidate(void) {
    size_t count;
    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    count = (size_t)tb_global.width * (size_t)tb_global.height;
    memset(tb_global.front, 0, count * sizeof(struct tb_cell));
    return TB_OK;
}

int tb_set_cursor(int cx, int cy) {
    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    tb_global.cursor_x = cx;
    tb_global.cursor_y = cy;
    return TB_OK;
}

int tb_hide_cursor(void) {
    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    tb_global.cursor_x = -1;
    tb_global.cursor_y = -1;
    return TB_OK;
}

int tb_set_cell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg) {
    size_t index;

    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    if (x < 0 || y < 0 || x >= tb_global.width || y >= tb_global.height) {
        return TB_ERR_OUT_OF_BOUNDS;
    }

    index = (size_t)y * (size_t)tb_global.width + (size_t)x;
    tb_fill_cell(&tb_global.back[index], ch, fg, bg);
    return TB_OK;
}

int tb_set_input_mode(int mode) {
    if (mode == TB_INPUT_CURRENT) {
        return tb_global.input_mode;
    }
    tb_global.input_mode = mode;
    return TB_OK;
}

int tb_set_output_mode(int mode) {
    if (mode == TB_OUTPUT_CURRENT) {
        return tb_global.output_mode;
    }
    tb_global.output_mode = mode;
    return TB_OK;
}

int tb_print(int x, int y, uintattr_t fg, uintattr_t bg, const char *str) {
    uint32_t ch;
    int len;

    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    if (str == (const char *)0) {
        return TB_OK;
    }

    while (*str != '\0') {
        len = tb_utf8_char_to_unicode(&ch, str);
        if (len <= 0) {
            ch = (uint32_t)'?';
            len = 1;
        }

        if (ch == '\n') {
            y++;
            x = 0;
        } else if (x >= 0 && y >= 0 && x < tb_global.width && y < tb_global.height) {
            (void)tb_set_cell(x, y, ch, fg, bg);
            x++;
        } else {
            x++;
        }

        str += len;
    }
    return TB_OK;
}

int tb_printf(int x, int y, uintattr_t fg, uintattr_t bg, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (rc < 0) {
        return TB_ERR;
    }
    return tb_print(x, y, fg, bg, buf);
}

int tb_peek_event(struct tb_event *event, int timeout_ms) {
    char ch;
    char seq[16];
    size_t seq_len = 0U;
    int rc;

    if (tb_global.initialized == 0) {
        return TB_ERR_NOT_INIT;
    }
    if (event == (struct tb_event *)0) {
        return TB_ERR;
    }

    if (tb_refresh_size_if_needed(event) == TB_OK && event->type == TB_EVENT_RESIZE) {
        return TB_EVENT_RESIZE;
    }

    memset(event, 0, sizeof(*event));
    rc = tb_read_byte_blocking(&ch, timeout_ms);
    if (rc != TB_OK) {
        return rc;
    }

    if ((unsigned char)ch == 27U) {
        seq[seq_len++] = ch;
        rc = tb_read_byte_blocking(&ch, 20);
        if (rc != TB_OK) {
            event->type = TB_EVENT_KEY;
            event->key = TB_KEY_ESC;
            return TB_EVENT_KEY;
        }
        seq[seq_len++] = ch;
        while (seq_len < sizeof(seq)) {
            if ((ch >= 'A' && ch <= 'Z') || ch == '~') {
                break;
            }
            rc = tb_read_byte_blocking(&ch, 20);
            if (rc != TB_OK) {
                break;
            }
            seq[seq_len++] = ch;
            if ((ch >= 'A' && ch <= 'Z') || ch == '~') {
                break;
            }
        }
        (void)tb_map_escape_sequence(seq, seq_len, event);
        return event->type;
    }

    event->type = TB_EVENT_KEY;
    if ((unsigned char)ch < 32U || (unsigned char)ch == 127U) {
        event->key = (uint16_t)(unsigned char)ch;
    } else {
        event->ch = (uint32_t)(unsigned char)ch;
    }
    return TB_EVENT_KEY;
}

int tb_poll_event(struct tb_event *event) {
    return tb_peek_event(event, -1);
}

const char *tb_strerror(int err) {
    switch (err) {
        case TB_OK:
            return "Success";
        case TB_ERR_INIT_ALREADY:
            return "Termbox initialized already";
        case TB_ERR_MEM:
            return "Out of memory";
        case TB_ERR_NO_EVENT:
            return "No event";
        case TB_ERR_NOT_INIT:
            return "Not initialized";
        case TB_ERR_OUT_OF_BOUNDS:
            return "Out of bounds";
        default:
            return "Generic termbox2 error";
    }
}

const char *tb_version(void) {
    return TB_VERSION_STR;
}

int tb_last_errno(void) {
    return tb_global.last_errno;
}

struct tb_cell *tb_cell_buffer(void) {
    return tb_global.back;
}

int tb_utf8_char_to_unicode(uint32_t *out, const char *c) {
    unsigned char ch0;

    if (out == (uint32_t *)0 || c == (const char *)0 || c[0] == '\0') {
        return 0;
    }

    ch0 = (unsigned char)c[0];
    if (ch0 < 0x80U) {
        *out = (uint32_t)ch0;
        return 1;
    }
    if ((ch0 & 0xE0U) == 0xC0U && c[1] != '\0') {
        *out = (uint32_t)(((ch0 & 0x1FU) << 6) | ((unsigned char)c[1] & 0x3FU));
        return 2;
    }
    if ((ch0 & 0xF0U) == 0xE0U && c[1] != '\0' && c[2] != '\0') {
        *out = (uint32_t)(((ch0 & 0x0FU) << 12) | (((unsigned char)c[1] & 0x3FU) << 6) |
                          ((unsigned char)c[2] & 0x3FU));
        return 3;
    }
    if ((ch0 & 0xF8U) == 0xF0U && c[1] != '\0' && c[2] != '\0' && c[3] != '\0') {
        *out = (uint32_t)(((ch0 & 0x07U) << 18) | (((unsigned char)c[1] & 0x3FU) << 12) |
                          (((unsigned char)c[2] & 0x3FU) << 6) | ((unsigned char)c[3] & 0x3FU));
        return 4;
    }

    *out = 0xFFFDU;
    return 1;
}

int tb_utf8_unicode_to_char(char *out, uint32_t c) {
    if (out == (char *)0) {
        return 0;
    }

    if (c < 0x80U) {
        out[0] = (char)c;
        out[1] = '\0';
        return 1;
    }
    if (c < 0x800U) {
        out[0] = (char)(0xC0U | ((c >> 6) & 0x1FU));
        out[1] = (char)(0x80U | (c & 0x3FU));
        out[2] = '\0';
        return 2;
    }
    if (c < 0x10000U) {
        out[0] = (char)(0xE0U | ((c >> 12) & 0x0FU));
        out[1] = (char)(0x80U | ((c >> 6) & 0x3FU));
        out[2] = (char)(0x80U | (c & 0x3FU));
        out[3] = '\0';
        return 3;
    }

    out[0] = (char)(0xF0U | ((c >> 18) & 0x07U));
    out[1] = (char)(0x80U | ((c >> 12) & 0x3FU));
    out[2] = (char)(0x80U | ((c >> 6) & 0x3FU));
    out[3] = (char)(0x80U | (c & 0x3FU));
    out[4] = '\0';
    return 4;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)puts("[termbox2] dynamic library ready");
    return 0;
}
