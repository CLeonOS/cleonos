#include <cleonos_tui.h>

#include <cleonos_syscall.h>
#include <stdio.h>
#include <string.h>

#define TUI_FD_STDOUT 1
#define TUI_FD_STDIN 0

static int tui_active = 0;
static cleonos_tui_style tui_current_style = {CLEONOS_TUI_COLOR_DEFAULT, CLEONOS_TUI_COLOR_DEFAULT, 0U};

static tui_u64 tui_strlen(const char *text) {
    tui_u64 len = 0ULL;

    if (text == (const char *)0) {
        return 0ULL;
    }

    while (text[len] != '\0') {
        len++;
    }

    return len;
}

static int tui_min_int(int a, int b) {
    return (a < b) ? a : b;
}

static int tui_max_int(int a, int b) {
    return (a > b) ? a : b;
}

static void tui_write_raw(const char *text) {
    tui_u64 len = tui_strlen(text);

    if (len == 0ULL) {
        return;
    }

    (void)cleonos_sys_fd_write((tui_u64)TUI_FD_STDOUT, text, len);
}

static void tui_write_char(char ch) {
    (void)cleonos_sys_fd_write((tui_u64)TUI_FD_STDOUT, &ch, 1ULL);
}

static tui_u64 tui_append_dec(char *out, tui_u64 out_size, int value) {
    unsigned int magnitude;
    unsigned int digits = 0U;
    char rev[16];
    unsigned int i;
    tui_u64 at = 0ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    if (value < 0) {
        if (at + 1ULL >= out_size) {
            return at;
        }
        out[at++] = '-';
        magnitude = (unsigned int)(-(value + 1)) + 1U;
    } else {
        magnitude = (unsigned int)value;
    }

    if (magnitude == 0U) {
        if (at + 1ULL < out_size) {
            out[at++] = '0';
        }
        out[at] = '\0';
        return at;
    }

    while (magnitude != 0U && digits < (unsigned int)sizeof(rev)) {
        rev[digits++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }

    for (i = 0U; i < digits; i++) {
        if (at + 1ULL >= out_size) {
            break;
        }
        out[at++] = rev[digits - 1U - i];
    }
    out[at] = '\0';
    return at;
}

static void tui_write_dec(int value) {
    char buf[24];

    (void)tui_append_dec(buf, (tui_u64)sizeof(buf), value);
    tui_write_raw(buf);
}

static int tui_color_to_ansi(int color, int background) {
    int base = (background != 0) ? 40 : 30;

    if (color == CLEONOS_TUI_COLOR_DEFAULT) {
        return (background != 0) ? 49 : 39;
    }

    if (color < CLEONOS_TUI_COLOR_BLACK || color > CLEONOS_TUI_COLOR_WHITE) {
        color = CLEONOS_TUI_COLOR_DEFAULT;
    }

    if (color == CLEONOS_TUI_COLOR_DEFAULT) {
        return (background != 0) ? 49 : 39;
    }

    return base + color;
}

cleonos_tui_style cleonos_tui_make_style(int fg, int bg, unsigned int attrs) {
    cleonos_tui_style style;

    style.fg = fg;
    style.bg = bg;
    style.attrs = attrs;
    return style;
}

void cleonos_tui_move(int y, int x) {
    char seq[32];
    tui_u64 at = 0ULL;

    if (y < 0) {
        y = 0;
    }
    if (x < 0) {
        x = 0;
    }

    seq[at++] = '\x1B';
    seq[at++] = '[';
    at += tui_append_dec(seq + at, (tui_u64)sizeof(seq) - at, y + 1);
    if (at + 2ULL >= (tui_u64)sizeof(seq)) {
        return;
    }
    seq[at++] = ';';
    at += tui_append_dec(seq + at, (tui_u64)sizeof(seq) - at, x + 1);
    if (at + 2ULL >= (tui_u64)sizeof(seq)) {
        return;
    }
    seq[at++] = 'H';
    seq[at] = '\0';
    tui_write_raw(seq);
}

void cleonos_tui_reset_style(void) {
    tui_current_style = cleonos_tui_make_style(CLEONOS_TUI_COLOR_DEFAULT, CLEONOS_TUI_COLOR_DEFAULT, 0U);
    tui_write_raw("\x1B[0m");
}

void cleonos_tui_set_style(cleonos_tui_style style) {
    int fg = tui_color_to_ansi(style.fg, 0);
    int bg = tui_color_to_ansi(style.bg, 1);
    char seq[48];
    tui_u64 at = 0ULL;

    tui_current_style = style;
    seq[at++] = '\x1B';
    seq[at++] = '[';
    seq[at++] = '0';
    if ((style.attrs & CLEONOS_TUI_ATTR_BOLD) != 0U) {
        seq[at++] = ';';
        seq[at++] = '1';
    }
    if ((style.attrs & CLEONOS_TUI_ATTR_UNDERLINE) != 0U) {
        seq[at++] = ';';
        seq[at++] = '4';
    }
    if ((style.attrs & CLEONOS_TUI_ATTR_REVERSE) != 0U) {
        seq[at++] = ';';
        seq[at++] = '7';
    }
    if (at + 4ULL >= (tui_u64)sizeof(seq)) {
        tui_write_raw("\x1B[0m");
        return;
    }
    seq[at++] = ';';
    at += tui_append_dec(seq + at, (tui_u64)sizeof(seq) - at, fg);
    if (at + 4ULL >= (tui_u64)sizeof(seq)) {
        tui_write_raw("\x1B[0m");
        return;
    }
    seq[at++] = ';';
    at += tui_append_dec(seq + at, (tui_u64)sizeof(seq) - at, bg);
    if (at + 2ULL >= (tui_u64)sizeof(seq)) {
        tui_write_raw("\x1B[0m");
        return;
    }
    seq[at++] = 'm';
    seq[at] = '\0';
    tui_write_raw(seq);
}

void cleonos_tui_clear(void) {
    tui_write_raw("\x1B[2J\x1B[H");
}

void cleonos_tui_refresh(void) {
    (void)tui_current_style;
}

void cleonos_tui_init(void) {
    tui_active = 1;
    cleonos_tui_clear();
    cleonos_tui_reset_style();
}

void cleonos_tui_shutdown(void) {
    if (tui_active == 0) {
        return;
    }

    cleonos_tui_reset_style();
    tui_write_raw("\n");
    tui_active = 0;
}

void cleonos_tui_puts_at(int y, int x, const char *text) {
    cleonos_tui_move(y, x);
    tui_write_raw(text);
}

void cleonos_tui_hline(int y, int x, int width, char ch, cleonos_tui_style style) {
    int i;

    if (width <= 0) {
        return;
    }

    cleonos_tui_set_style(style);
    cleonos_tui_move(y, x);
    for (i = 0; i < width; i++) {
        tui_write_char(ch);
    }
    cleonos_tui_reset_style();
}

void cleonos_tui_vline(int y, int x, int height, char ch, cleonos_tui_style style) {
    int i;

    if (height <= 0) {
        return;
    }

    cleonos_tui_set_style(style);
    for (i = 0; i < height; i++) {
        cleonos_tui_move(y + i, x);
        tui_write_char(ch);
    }
    cleonos_tui_reset_style();
}

void cleonos_tui_fill_rect(cleonos_tui_rect rect, char ch, cleonos_tui_style style) {
    int row;
    int col;

    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }

    cleonos_tui_set_style(style);
    for (row = 0; row < rect.height; row++) {
        cleonos_tui_move(rect.y + row, rect.x);
        for (col = 0; col < rect.width; col++) {
            tui_write_char(ch);
        }
    }
    cleonos_tui_reset_style();
}

void cleonos_tui_box(cleonos_tui_rect rect, const char *title, cleonos_tui_style style) {
    int right;
    int bottom;
    int title_len;
    int copy_len;

    if (rect.width < 2 || rect.height < 2) {
        return;
    }

    right = rect.x + rect.width - 1;
    bottom = rect.y + rect.height - 1;
    cleonos_tui_set_style(style);

    cleonos_tui_move(rect.y, rect.x);
    tui_write_char('+');
    for (int x = rect.x + 1; x < right; x++) {
        tui_write_char('-');
    }
    tui_write_char('+');

    for (int y = rect.y + 1; y < bottom; y++) {
        cleonos_tui_move(y, rect.x);
        tui_write_char('|');
        cleonos_tui_move(y, right);
        tui_write_char('|');
    }

    cleonos_tui_move(bottom, rect.x);
    tui_write_char('+');
    for (int x = rect.x + 1; x < right; x++) {
        tui_write_char('-');
    }
    tui_write_char('+');

    if (title != (const char *)0 && title[0] != '\0' && rect.width > 6) {
        title_len = (int)tui_strlen(title);
        copy_len = tui_min_int(title_len, rect.width - 6);
        cleonos_tui_move(rect.y, rect.x + 2);
        tui_write_char(' ');
        for (int i = 0; i < copy_len; i++) {
            tui_write_char(title[i]);
        }
        tui_write_char(' ');
    }

    cleonos_tui_reset_style();
}

void cleonos_tui_button(cleonos_tui_rect rect, const char *label, int focused) {
    cleonos_tui_style style;
    int label_len;
    int start_x;

    if (rect.width < 4 || rect.height < 1) {
        return;
    }

    style = focused != 0 ? cleonos_tui_make_style(CLEONOS_TUI_COLOR_BLACK, CLEONOS_TUI_COLOR_CYAN, CLEONOS_TUI_ATTR_BOLD)
                         : cleonos_tui_make_style(CLEONOS_TUI_COLOR_WHITE, CLEONOS_TUI_COLOR_BLUE, 0U);
    cleonos_tui_fill_rect(rect, ' ', style);

    if (label == (const char *)0) {
        label = "";
    }

    label_len = (int)tui_strlen(label);
    label_len = tui_min_int(label_len, rect.width - 2);
    start_x = rect.x + tui_max_int(1, (rect.width - label_len) / 2);

    cleonos_tui_set_style(style);
    cleonos_tui_move(rect.y + (rect.height / 2), start_x);
    for (int i = 0; i < label_len; i++) {
        tui_write_char(label[i]);
    }
    cleonos_tui_reset_style();
}

void cleonos_tui_status_bar(int y, const char *left, const char *right) {
    cleonos_tui_style style = cleonos_tui_make_style(CLEONOS_TUI_COLOR_BLACK, CLEONOS_TUI_COLOR_CYAN, CLEONOS_TUI_ATTR_BOLD);
    int width = 80;
    int right_len = (int)tui_strlen(right);
    int right_x = width - right_len - 1;

    cleonos_tui_fill_rect((cleonos_tui_rect){0, y, width, 1}, ' ', style);
    cleonos_tui_set_style(style);
    cleonos_tui_move(y, 1);
    tui_write_raw(left);
    if (right != (const char *)0 && right_x > 1) {
        cleonos_tui_move(y, right_x);
        tui_write_raw(right);
    }
    cleonos_tui_reset_style();
}

int cleonos_tui_read_key(void) {
    char ch = '\0';
    u64 got;

    for (;;) {
        got = cleonos_sys_fd_read((tui_u64)TUI_FD_STDIN, &ch, 1ULL);
        if (got == 1ULL) {
            return (int)(unsigned char)ch;
        }
        if (got == (tui_u64)-1) {
            return -1;
        }
        (void)cleonos_sys_yield();
    }
}

const char *cleonos_tui_key_name(int key, char *out, tui_u64 out_size) {
    if (out == (char *)0 || out_size == 0ULL) {
        return "";
    }

    if (key < 0) {
        (void)snprintf(out, (unsigned long)out_size, "EOF");
    } else if (key == 27) {
        (void)snprintf(out, (unsigned long)out_size, "ESC");
    } else if (key == '\n' || key == '\r') {
        (void)snprintf(out, (unsigned long)out_size, "ENTER");
    } else if (key == '\t') {
        (void)snprintf(out, (unsigned long)out_size, "TAB");
    } else if (key == 8 || key == 127) {
        (void)snprintf(out, (unsigned long)out_size, "BACKSPACE");
    } else if (key >= 32 && key <= 126) {
        (void)snprintf(out, (unsigned long)out_size, "'%c'", key);
    } else {
        (void)snprintf(out, (unsigned long)out_size, "0x%02X", (unsigned int)(key & 0xFF));
    }

    return out;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    (void)puts("[tui] dynamic TUI library ready");
    return 0;
}
