#include "cmd_runtime.h"

#define USH_VIM_MAX_LINES 2048ULL
#define USH_VIM_MAX_LINE_LEN 512ULL
#define USH_VIM_STATUS_MAX 160ULL
#define USH_VIM_CMD_MAX 128ULL
#define USH_VIM_VIEW_ROWS 20ULL
#define USH_VIM_VIEW_COLS 72ULL
#define USH_VIM_UI_COLS (5ULL + USH_VIM_VIEW_COLS)
#define USH_VIM_ROW_HEADER 1ULL
#define USH_VIM_ROW_POS 2ULL
#define USH_VIM_ROW_TOP_SEPARATOR 3ULL
#define USH_VIM_ROW_TEXT_BASE 4ULL
#define USH_VIM_ROW_BOTTOM_SEPARATOR (USH_VIM_ROW_TEXT_BASE + USH_VIM_VIEW_ROWS)
#define USH_VIM_ROW_FOOTER (USH_VIM_ROW_BOTTOM_SEPARATOR + 1ULL)

#define USH_VIM_MODE_NORMAL 0
#define USH_VIM_MODE_INSERT 1
#define USH_VIM_MODE_COMMAND 2

#define USH_VIM_KEY_ESC 27
#define USH_VIM_KEY_ENTER 13
#define USH_VIM_KEY_BACKSPACE 8
#define USH_VIM_KEY_DEL 127
#define USH_VIM_KEY_UP 1001
#define USH_VIM_KEY_DOWN 1002
#define USH_VIM_KEY_LEFT 1003
#define USH_VIM_KEY_RIGHT 1004
#define USH_VIM_KEY_HOME 1005
#define USH_VIM_KEY_END 1006

typedef struct ush_vim_line {
    char text[USH_VIM_MAX_LINE_LEN];
    u64 len;
} ush_vim_line;

typedef struct ush_vim_editor {
    ush_vim_line lines[USH_VIM_MAX_LINES];
    u64 line_count;
    u64 cursor_line;
    u64 cursor_col;
    u64 scroll_top;
    int mode;
    int modified;
    int quit_requested;
    int pending_dd;
    int pending_gg;
    int pending_jj;
    char file_path[USH_PATH_MAX];
    char status[USH_VIM_STATUS_MAX];
    char command[USH_VIM_CMD_MAX];
    u64 command_len;
} ush_vim_editor;

static void ush_vim_set_status(ush_vim_editor *ed, const char *text) {
    if (ed == (ush_vim_editor *)0 || text == (const char *)0) {
        return;
    }

    ush_copy(ed->status, (u64)sizeof(ed->status), text);
}

static void ush_vim_set_statusf(ush_vim_editor *ed, const char *fmt, u64 value) {
    char line[USH_VIM_STATUS_MAX];

    if (ed == (ush_vim_editor *)0 || fmt == (const char *)0) {
        return;
    }

    (void)snprintf(line, sizeof(line), fmt, (unsigned long long)value);
    ush_vim_set_status(ed, line);
}

static int ush_vim_utf8_is_cont(unsigned char ch) {
    return (ch >= 0x80U && ch <= 0xBFU) ? 1 : 0;
}

static u64 ush_vim_utf8_len_from_lead(unsigned char lead) {
    if (lead < 0x80U) {
        return 1ULL;
    }
    if (lead >= 0xC2U && lead <= 0xDFU) {
        return 2ULL;
    }
    if (lead >= 0xE0U && lead <= 0xEFU) {
        return 3ULL;
    }
    if (lead >= 0xF0U && lead <= 0xF4U) {
        return 4ULL;
    }
    return 1ULL;
}

static u64 ush_vim_utf8_char_len_at(const char *text, u64 len, u64 pos) {
    u64 need;
    u64 i;

    if (text == (const char *)0 || pos >= len) {
        return 0ULL;
    }

    need = ush_vim_utf8_len_from_lead((unsigned char)text[pos]);
    if (need == 1ULL || pos + need > len) {
        return 1ULL;
    }

    for (i = 1ULL; i < need; i++) {
        if (ush_vim_utf8_is_cont((unsigned char)text[pos + i]) == 0) {
            return 1ULL;
        }
    }

    return need;
}

static unsigned int ush_vim_utf8_decode_at(const char *text, u64 len, u64 pos, u64 *out_adv) {
    unsigned char b0;
    u64 adv;
    unsigned int cp;

    if (out_adv != (u64 *)0) {
        *out_adv = 0ULL;
    }
    if (text == (const char *)0 || pos >= len) {
        return 0U;
    }

    b0 = (unsigned char)text[pos];
    adv = ush_vim_utf8_char_len_at(text, len, pos);
    if (out_adv != (u64 *)0) {
        *out_adv = adv;
    }

    if (adv == 1ULL) {
        return (unsigned int)b0;
    }
    if (adv == 2ULL) {
        return ((unsigned int)(b0 & 0x1FU) << 6) | (unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU);
    }
    if (adv == 3ULL) {
        return ((unsigned int)(b0 & 0x0FU) << 12) |
               ((unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU) << 6) |
               (unsigned int)((unsigned char)text[pos + 2ULL] & 0x3FU);
    }

    cp = ((unsigned int)(b0 & 0x07U) << 18) |
         ((unsigned int)((unsigned char)text[pos + 1ULL] & 0x3FU) << 12) |
         ((unsigned int)((unsigned char)text[pos + 2ULL] & 0x3FU) << 6) |
         (unsigned int)((unsigned char)text[pos + 3ULL] & 0x3FU);
    return cp;
}

static u64 ush_vim_codepoint_width(unsigned int cp) {
    if (cp == 0U) {
        return 0ULL;
    }
    if ((cp >= 0x0300U && cp <= 0x036FU) || (cp >= 0xFE00U && cp <= 0xFE0FU)) {
        return 0ULL;
    }
    if ((cp >= 0x1100U && cp <= 0x115FU) || (cp >= 0x2E80U && cp <= 0xA4CFU) ||
        (cp >= 0xAC00U && cp <= 0xD7A3U) || (cp >= 0xF900U && cp <= 0xFAFFU) ||
        (cp >= 0xFE10U && cp <= 0xFE19U) || (cp >= 0xFE30U && cp <= 0xFE6FU) ||
        (cp >= 0xFF00U && cp <= 0xFF60U) || (cp >= 0xFFE0U && cp <= 0xFFE6U) ||
        (cp >= 0x20000U && cp <= 0x3FFFDU)) {
        return 2ULL;
    }
    return 1ULL;
}

static u64 ush_vim_utf8_visual_width_n(const char *text, u64 len) {
    u64 pos = 0ULL;
    u64 cols = 0ULL;

    while (text != (const char *)0 && pos < len) {
        u64 adv = 1ULL;
        unsigned int cp = ush_vim_utf8_decode_at(text, len, pos, &adv);
        if (adv == 0ULL) {
            break;
        }
        cols += ush_vim_codepoint_width(cp);
        pos += adv;
    }

    return cols;
}

static u64 ush_vim_utf8_prev_boundary(const char *text, u64 len, u64 pos) {
    if (text == (const char *)0 || pos == 0ULL) {
        return 0ULL;
    }
    if (pos > len) {
        pos = len;
    }

    pos--;
    while (pos > 0ULL && ush_vim_utf8_is_cont((unsigned char)text[pos]) != 0) {
        pos--;
    }

    return pos;
}

static u64 ush_vim_utf8_next_boundary(const char *text, u64 len, u64 pos) {
    u64 adv;

    if (text == (const char *)0 || pos >= len) {
        return len;
    }

    adv = ush_vim_utf8_char_len_at(text, len, pos);
    if (adv == 0ULL) {
        adv = 1ULL;
    }
    if (pos + adv > len) {
        return len;
    }

    return pos + adv;
}

static char ush_vim_read_byte_blocking(void) {
    char ch = '\0';
    u64 got;

    for (;;) {
        got = cleonos_sys_fd_read(0ULL, &ch, 1ULL);
        if (got == 1ULL) {
            return ch;
        }
        (void)cleonos_sys_sleep_ticks(1ULL);
    }
}

static u64 ush_vim_collect_utf8(int first, char *out, u64 out_size) {
    u64 need;
    u64 len = 1ULL;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    out[0] = (char)first;
    need = ush_vim_utf8_len_from_lead((unsigned char)first);
    if (need > out_size) {
        need = out_size;
    }

    while (len < need) {
        char next = ush_vim_read_byte_blocking();
        out[len++] = next;
        if (ush_vim_utf8_is_cont((unsigned char)next) == 0) {
            break;
        }
    }

    return len;
}

static void ush_vim_reset_buffer(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    ush_zero(ed->lines, (u64)sizeof(ed->lines));
    ed->line_count = 1ULL;
    ed->cursor_line = 0ULL;
    ed->cursor_col = 0ULL;
    ed->scroll_top = 0ULL;
}

static void ush_vim_init(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    ush_zero(ed, (u64)sizeof(*ed));
    ed->mode = USH_VIM_MODE_NORMAL;
    ush_vim_reset_buffer(ed);
    ush_vim_set_status(ed, "Ready. Press i to insert (jj => NORMAL), :w to save, :q to quit.");
}

static void ush_vim_keep_cursor_valid(ush_vim_editor *ed) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0 || ed->line_count == 0ULL) {
        return;
    }

    if (ed->cursor_line >= ed->line_count) {
        ed->cursor_line = ed->line_count - 1ULL;
    }

    line = &ed->lines[ed->cursor_line];
    if (ed->cursor_col > line->len) {
        ed->cursor_col = line->len;
    }
    while (ed->cursor_col > 0ULL && ed->cursor_col < line->len &&
           ush_vim_utf8_is_cont((unsigned char)line->text[ed->cursor_col]) != 0) {
        ed->cursor_col--;
    }
}

static void ush_vim_ensure_cursor_visible(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (ed->cursor_line < ed->scroll_top) {
        ed->scroll_top = ed->cursor_line;
    } else if (ed->cursor_line >= ed->scroll_top + USH_VIM_VIEW_ROWS) {
        ed->scroll_top = ed->cursor_line - USH_VIM_VIEW_ROWS + 1ULL;
    }
}

static void ush_vim_move_left(ush_vim_editor *ed) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0) {
        return;
    }

    line = &ed->lines[ed->cursor_line];
    if (ed->cursor_col > 0ULL) {
        ed->cursor_col = ush_vim_utf8_prev_boundary(line->text, line->len, ed->cursor_col);
    } else if (ed->cursor_line > 0ULL) {
        ed->cursor_line--;
        ed->cursor_col = ed->lines[ed->cursor_line].len;
    }
}

static void ush_vim_move_right(ush_vim_editor *ed) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0) {
        return;
    }

    line = &ed->lines[ed->cursor_line];
    if (ed->cursor_col < line->len) {
        ed->cursor_col = ush_vim_utf8_next_boundary(line->text, line->len, ed->cursor_col);
    } else if (ed->cursor_line + 1ULL < ed->line_count) {
        ed->cursor_line++;
        ed->cursor_col = 0ULL;
    }
}

static void ush_vim_move_up(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (ed->cursor_line > 0ULL) {
        ed->cursor_line--;
    }

    ush_vim_keep_cursor_valid(ed);
}

static void ush_vim_move_down(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (ed->cursor_line + 1ULL < ed->line_count) {
        ed->cursor_line++;
    }

    ush_vim_keep_cursor_valid(ed);
}

static int ush_vim_insert_line_after(ush_vim_editor *ed, u64 line_index) {
    u64 i;

    if (ed == (ush_vim_editor *)0 || line_index >= ed->line_count) {
        return 0;
    }

    if (ed->line_count >= USH_VIM_MAX_LINES) {
        ush_vim_set_status(ed, "line limit reached");
        return 0;
    }

    for (i = ed->line_count; i > line_index + 1ULL; i--) {
        ed->lines[i] = ed->lines[i - 1ULL];
    }

    ush_zero(&ed->lines[line_index + 1ULL], (u64)sizeof(ed->lines[line_index + 1ULL]));
    ed->line_count++;
    return 1;
}

static int ush_vim_insert_bytes(ush_vim_editor *ed, const char *bytes, u64 byte_len) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0 || bytes == (const char *)0 || byte_len == 0ULL) {
        return 0;
    }

    if ((unsigned char)bytes[0] < 0x20U || bytes[0] == '\n' || bytes[0] == '\r') {
        return 0;
    }

    line = &ed->lines[ed->cursor_line];
    if (line->len + byte_len >= USH_VIM_MAX_LINE_LEN) {
        ush_vim_set_status(ed, "line too long");
        return 0;
    }

    if (ed->cursor_col < line->len) {
        (void)memmove(line->text + ed->cursor_col + byte_len, line->text + ed->cursor_col,
                      (size_t)(line->len - ed->cursor_col));
    }
    (void)memcpy(line->text + ed->cursor_col, bytes, (size_t)byte_len);
    line->len += byte_len;
    line->text[line->len] = '\0';
    ed->cursor_col += byte_len;
    ed->modified = 1;
    return 1;
}

static int ush_vim_insert_char(ush_vim_editor *ed, char ch) {
    return ush_vim_insert_bytes(ed, &ch, 1ULL);
}

static int ush_vim_split_line(ush_vim_editor *ed) {
    ush_vim_line *line;
    ush_vim_line *next;
    u64 tail_len;

    if (ed == (ush_vim_editor *)0) {
        return 0;
    }

    if (ush_vim_insert_line_after(ed, ed->cursor_line) == 0) {
        return 0;
    }

    line = &ed->lines[ed->cursor_line];
    next = &ed->lines[ed->cursor_line + 1ULL];
    tail_len = line->len - ed->cursor_col;

    if (tail_len > 0ULL) {
        (void)memcpy(next->text, line->text + ed->cursor_col, (size_t)tail_len);
        next->len = tail_len;
        next->text[next->len] = '\0';
        line->len = ed->cursor_col;
        line->text[line->len] = '\0';
    }

    ed->cursor_line++;
    ed->cursor_col = 0ULL;
    ed->modified = 1;
    return 1;
}

static int ush_vim_backspace(ush_vim_editor *ed) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0) {
        return 0;
    }

    line = &ed->lines[ed->cursor_line];
    if (ed->cursor_col > 0ULL) {
        u64 delete_start = ush_vim_utf8_prev_boundary(line->text, line->len, ed->cursor_col);
        u64 delete_len = ed->cursor_col - delete_start;
        if (delete_len == 0ULL) {
            delete_len = 1ULL;
        }
        if (ed->cursor_col < line->len) {
            (void)memmove(line->text + delete_start, line->text + ed->cursor_col,
                          (size_t)(line->len - ed->cursor_col));
        }
        line->len -= delete_len;
        line->text[line->len] = '\0';
        ed->cursor_col = delete_start;
        ed->modified = 1;
        return 1;
    }

    if (ed->cursor_line == 0ULL) {
        return 0;
    }

    {
        ush_vim_line *prev = &ed->lines[ed->cursor_line - 1ULL];
        u64 curr_len = line->len;
        u64 i;

        if (prev->len + curr_len >= USH_VIM_MAX_LINE_LEN) {
            ush_vim_set_status(ed, "merge would exceed line length");
            return 0;
        }

        (void)memcpy(prev->text + prev->len, line->text, (size_t)curr_len);
        prev->len += curr_len;
        prev->text[prev->len] = '\0';

        for (i = ed->cursor_line; i + 1ULL < ed->line_count; i++) {
            ed->lines[i] = ed->lines[i + 1ULL];
        }
        ush_zero(&ed->lines[ed->line_count - 1ULL], (u64)sizeof(ed->lines[ed->line_count - 1ULL]));
        ed->line_count--;
        ed->cursor_line--;
        ed->cursor_col = prev->len - curr_len;
        ed->modified = 1;
    }

    return 1;
}

static int ush_vim_delete_char(ush_vim_editor *ed) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0) {
        return 0;
    }

    line = &ed->lines[ed->cursor_line];
    if (ed->cursor_col >= line->len) {
        return 0;
    }

    {
        u64 next = ush_vim_utf8_next_boundary(line->text, line->len, ed->cursor_col);
        u64 delete_len = next - ed->cursor_col;
        if (delete_len == 0ULL) {
            delete_len = 1ULL;
        }

        if (ed->cursor_col + delete_len < line->len) {
            (void)memmove(line->text + ed->cursor_col, line->text + ed->cursor_col + delete_len,
                          (size_t)(line->len - ed->cursor_col - delete_len));
        }
        line->len -= delete_len;
    }
    line->text[line->len] = '\0';
    ed->modified = 1;
    return 1;
}

static int ush_vim_delete_line(ush_vim_editor *ed) {
    u64 i;

    if (ed == (ush_vim_editor *)0 || ed->line_count == 0ULL) {
        return 0;
    }

    if (ed->line_count == 1ULL) {
        ed->lines[0].len = 0ULL;
        ed->lines[0].text[0] = '\0';
        ed->cursor_line = 0ULL;
        ed->cursor_col = 0ULL;
        ed->modified = 1;
        return 1;
    }

    for (i = ed->cursor_line; i + 1ULL < ed->line_count; i++) {
        ed->lines[i] = ed->lines[i + 1ULL];
    }

    ush_zero(&ed->lines[ed->line_count - 1ULL], (u64)sizeof(ed->lines[ed->line_count - 1ULL]));
    ed->line_count--;
    if (ed->cursor_line >= ed->line_count) {
        ed->cursor_line = ed->line_count - 1ULL;
    }
    ush_vim_keep_cursor_valid(ed);
    ed->modified = 1;
    return 1;
}

static int ush_vim_open_below_insert(ush_vim_editor *ed) {
    if (ed == (ush_vim_editor *)0) {
        return 0;
    }

    if (ush_vim_insert_line_after(ed, ed->cursor_line) == 0) {
        return 0;
    }

    ed->cursor_line++;
    ed->cursor_col = 0ULL;
    ed->mode = USH_VIM_MODE_INSERT;
    ed->modified = 1;
    return 1;
}

static int ush_vim_read_key(void) {
    char ch = '\0';
    u64 got;
    unsigned char code;

    for (;;) {
        got = cleonos_sys_fd_read(0ULL, &ch, 1ULL);
        if (got == 1ULL) {
            break;
        }

        /*
         * On some tty backends, no-input may be reported as 0 or -1.
         * Treat both as transient and wait instead of redrawing forever.
         */
        (void)cleonos_sys_sleep_ticks(1ULL);
    }

    if (ch == '\r') {
        return USH_VIM_KEY_ENTER;
    }

    if ((unsigned char)ch == USH_VIM_KEY_BACKSPACE || (unsigned char)ch == USH_VIM_KEY_DEL) {
        return USH_VIM_KEY_BACKSPACE;
    }

    code = (unsigned char)ch;
    if (code == 0x01U) {
        return USH_VIM_KEY_LEFT;
    }
    if (code == 0x02U) {
        return USH_VIM_KEY_RIGHT;
    }
    if (code == 0x03U) {
        return USH_VIM_KEY_UP;
    }
    if (code == 0x04U) {
        return USH_VIM_KEY_DOWN;
    }
    if (code == 0x05U) {
        return USH_VIM_KEY_HOME;
    }
    if (code == 0x06U) {
        return USH_VIM_KEY_END;
    }
    if (code == 0x07U) {
        return USH_VIM_KEY_DEL;
    }

    return (int)code;
}

static void ush_vim_write_padded_line_no_newline(const char *text) {
    u64 len = 0ULL;
    u64 i;

    if (text != (const char *)0) {
        len = ush_strlen(text);
        ush_write(text);
    }

    if (len < USH_VIM_UI_COLS) {
        for (i = len; i < USH_VIM_UI_COLS; i++) {
            ush_write_char(' ');
        }
    }
}

static void ush_vim_write_separator_no_newline(void) {
    u64 i;

    for (i = 0ULL; i < USH_VIM_UI_COLS; i++) {
        ush_write_char('-');
    }
}

static void ush_vim_goto(u64 row, u64 col) {
    char seq[48];

    (void)snprintf(seq, sizeof(seq), "\x1B[%llu;%lluH", (unsigned long long)row, (unsigned long long)col);
    ush_write(seq);
}

static void ush_vim_render_line_no_newline(const ush_vim_editor *ed, u64 line_index) {
    char buf[256];
    u64 col;
    u64 pos;
    const ush_vim_line *line;
    u64 visual_line_no;

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    if (line_index >= ed->line_count) {
        ush_write("~");
        for (col = 1ULL; col < USH_VIM_UI_COLS; col++) {
            ush_write_char(' ');
        }
        return;
    }

    line = &ed->lines[line_index];
    visual_line_no = line_index + 1ULL;
    (void)snprintf(buf, sizeof(buf), "%4llu ", (unsigned long long)visual_line_no);
    ush_write(buf);

    col = 0ULL;
    pos = 0ULL;
    while (pos < line->len && col < USH_VIM_VIEW_COLS) {
        u64 adv = 1ULL;
        u64 width;
        unsigned int cp = ush_vim_utf8_decode_at(line->text, line->len, pos, &adv);

        if (adv == 0ULL) {
            break;
        }

        width = ush_vim_codepoint_width(cp);
        if (width == 0ULL) {
            width = 1ULL;
        }
        if (col + width > USH_VIM_VIEW_COLS) {
            break;
        }

        (void)cleonos_sys_fd_write(1ULL, line->text + pos, adv);
        col += width;
        pos += adv;
    }

    while (col < USH_VIM_VIEW_COLS) {
        ush_write_char(' ');
        col++;
    }
}

static u64 ush_vim_line_cursor_visual_col(const ush_vim_line *line, u64 cursor_col) {
    if (line == (const ush_vim_line *)0) {
        return 0ULL;
    }
    if (cursor_col > line->len) {
        cursor_col = line->len;
    }
    while (cursor_col > 0ULL && ush_vim_utf8_is_cont((unsigned char)line->text[cursor_col]) != 0) {
        cursor_col--;
    }
    return ush_vim_utf8_visual_width_n(line->text, cursor_col);
}

static const char *ush_vim_mode_text(const ush_vim_editor *ed) {
    if (ed == (const ush_vim_editor *)0) {
        return "UNKNOWN";
    }

    if (ed->mode == USH_VIM_MODE_INSERT) {
        return "INSERT";
    }
    if (ed->mode == USH_VIM_MODE_COMMAND) {
        return "COMMAND";
    }

    return "NORMAL";
}

static void ush_vim_place_terminal_cursor(const ush_vim_editor *ed) {
    char seq[48];
    u64 row = 1ULL;
    u64 col = 1ULL;

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    if (ed->mode == USH_VIM_MODE_COMMAND) {
        row = USH_VIM_ROW_FOOTER;
        col = 2ULL + ed->command_len;
        if (col > USH_VIM_UI_COLS) {
            col = USH_VIM_UI_COLS;
        }
    } else {
        u64 visual_row = 0ULL;
        u64 visual_col = 0ULL;

        if (ed->cursor_line > ed->scroll_top) {
            visual_row = ed->cursor_line - ed->scroll_top;
        }
        if (ed->cursor_line < ed->line_count) {
            visual_col = ush_vim_line_cursor_visual_col(&ed->lines[ed->cursor_line], ed->cursor_col);
        }
        if (visual_row >= USH_VIM_VIEW_ROWS) {
            visual_row = USH_VIM_VIEW_ROWS - 1ULL;
        }
        if (visual_col >= USH_VIM_VIEW_COLS) {
            visual_col = USH_VIM_VIEW_COLS - 1ULL;
        }

        row = USH_VIM_ROW_TEXT_BASE + visual_row;
        col = 6ULL + visual_col;
    }

    (void)snprintf(seq, sizeof(seq), "\x1B[%llu;%lluH\x1B[?25h", (unsigned long long)row, (unsigned long long)col);
    ush_write(seq);
}

static void ush_vim_render_header_at(const ush_vim_editor *ed) {
    char header[256];
    const char *name;

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    name = (ed->file_path[0] != '\0') ? ed->file_path : "[No Name]";

    ush_vim_goto(USH_VIM_ROW_HEADER, 1ULL);
    (void)snprintf(header, sizeof(header), "vim clone | %s | %s%s", name, ush_vim_mode_text(ed),
                   (ed->modified != 0) ? " [+]" : "");
    ush_vim_write_padded_line_no_newline(header);
}

static void ush_vim_render_pos_at(const ush_vim_editor *ed) {
    char pos[128];

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    ush_vim_goto(USH_VIM_ROW_POS, 1ULL);
    (void)snprintf(pos, sizeof(pos), "line %llu/%llu col %llu", (unsigned long long)(ed->cursor_line + 1ULL),
                   (unsigned long long)ed->line_count, (unsigned long long)(ed->cursor_col + 1ULL));
    ush_vim_write_padded_line_no_newline(pos);
}

static void ush_vim_render_visual_line_at(const ush_vim_editor *ed, u64 visual_row) {
    if (ed == (const ush_vim_editor *)0 || visual_row >= USH_VIM_VIEW_ROWS) {
        return;
    }

    ush_vim_goto(USH_VIM_ROW_TEXT_BASE + visual_row, 1ULL);
    ush_vim_render_line_no_newline(ed, ed->scroll_top + visual_row);
}

static void ush_vim_render_footer_at(const ush_vim_editor *ed) {
    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    ush_vim_goto(USH_VIM_ROW_FOOTER, 1ULL);
    if (ed->mode == USH_VIM_MODE_COMMAND) {
        char cmdline[USH_VIM_CMD_MAX + 2ULL];

        cmdline[0] = ':';
        cmdline[1] = '\0';
        ush_copy(cmdline + 1ULL, (u64)sizeof(cmdline) - 1ULL, ed->command);
        ush_vim_write_padded_line_no_newline(cmdline);
    } else {
        ush_vim_write_padded_line_no_newline(ed->status);
    }
}

static void ush_vim_render(const ush_vim_editor *ed) {
    u64 row;

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    ush_write("\x1B[?25l");
    ush_vim_render_header_at(ed);
    ush_vim_render_pos_at(ed);
    ush_vim_goto(USH_VIM_ROW_TOP_SEPARATOR, 1ULL);
    ush_vim_write_separator_no_newline();

    for (row = 0ULL; row < USH_VIM_VIEW_ROWS; row++) {
        ush_vim_render_visual_line_at(ed, row);
    }

    ush_vim_goto(USH_VIM_ROW_BOTTOM_SEPARATOR, 1ULL);
    ush_vim_write_separator_no_newline();
    ush_vim_render_footer_at(ed);

    ush_vim_place_terminal_cursor(ed);
}

static void ush_vim_render_partial(const ush_vim_editor *ed, int header_dirty, int pos_dirty, int footer_dirty,
                                   int line_dirty, u64 line_index) {
    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    ush_write("\x1B[?25l");
    if (header_dirty != 0) {
        ush_vim_render_header_at(ed);
    }
    if (pos_dirty != 0) {
        ush_vim_render_pos_at(ed);
    }
    if (line_dirty != 0 && line_index >= ed->scroll_top &&
        line_index < ed->scroll_top + USH_VIM_VIEW_ROWS) {
        ush_vim_render_visual_line_at(ed, line_index - ed->scroll_top);
    }
    if (footer_dirty != 0) {
        ush_vim_render_footer_at(ed);
    }

    ush_vim_place_terminal_cursor(ed);
}

static int ush_vim_fd_write_all(u64 fd, const char *text, u64 len) {
    u64 done = 0ULL;

    if (text == (const char *)0) {
        return 0;
    }

    while (done < len) {
        u64 wrote = cleonos_sys_fd_write(fd, text + done, len - done);

        if (wrote == (u64)-1 || wrote == 0ULL) {
            return 0;
        }

        done += wrote;
    }

    return 1;
}

static int ush_vim_save_file(ush_vim_editor *ed, const char *path) {
    u64 fd;
    u64 i;
    u64 bytes = 0ULL;

    if (ed == (ush_vim_editor *)0 || path == (const char *)0 || path[0] == '\0') {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_WRONLY | CLEONOS_O_CREAT | CLEONOS_O_TRUNC, 0ULL);
    if (fd == (u64)-1) {
        ush_vim_set_status(ed, "write failed: cannot open target");
        return 0;
    }

    for (i = 0ULL; i < ed->line_count; i++) {
        const ush_vim_line *line = &ed->lines[i];

        if (line->len > 0ULL) {
            if (ush_vim_fd_write_all(fd, line->text, line->len) == 0) {
                (void)cleonos_sys_fd_close(fd);
                ush_vim_set_status(ed, "write failed: io error");
                return 0;
            }
            bytes += line->len;
        }

        if (i + 1ULL < ed->line_count) {
            if (ush_vim_fd_write_all(fd, "\n", 1ULL) == 0) {
                (void)cleonos_sys_fd_close(fd);
                ush_vim_set_status(ed, "write failed: io error");
                return 0;
            }
            bytes++;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    ush_copy(ed->file_path, (u64)sizeof(ed->file_path), path);
    ed->modified = 0;
    ush_vim_set_statusf(ed, "written bytes=%llu", bytes);
    return 1;
}

static int ush_vim_load_file(ush_vim_editor *ed, const char *path) {
    u64 fd;
    char buf[256];
    u64 got;
    u64 line_index = 0ULL;
    int truncated = 0;

    if (ed == (ush_vim_editor *)0 || path == (const char *)0 || path[0] == '\0') {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        ush_copy(ed->file_path, (u64)sizeof(ed->file_path), path);
        ush_vim_set_status(ed, "new file");
        return 1;
    }

    ush_vim_reset_buffer(ed);

    for (;;) {
        got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        if (got == 0ULL) {
            break;
        }
        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            ush_vim_set_status(ed, "read failed");
            return 0;
        }

        {
            u64 i;
            for (i = 0ULL; i < got; i++) {
                char ch = buf[i];
                ush_vim_line *line = &ed->lines[line_index];

                if (ch == '\r') {
                    continue;
                }

                if (ch == '\n') {
                    if (line_index + 1ULL < USH_VIM_MAX_LINES) {
                        line_index++;
                    } else {
                        truncated = 1;
                    }
                    continue;
                }

                if (line->len + 1ULL < USH_VIM_MAX_LINE_LEN) {
                    line->text[line->len++] = ch;
                    line->text[line->len] = '\0';
                } else {
                    truncated = 1;
                }
            }
        }
    }

    (void)cleonos_sys_fd_close(fd);
    ed->line_count = line_index + 1ULL;
    if (ed->line_count == 0ULL) {
        ed->line_count = 1ULL;
    }
    ed->cursor_line = 0ULL;
    ed->cursor_col = 0ULL;
    ed->scroll_top = 0ULL;
    ed->modified = 0;
    ush_copy(ed->file_path, (u64)sizeof(ed->file_path), path);

    if (truncated != 0) {
        ush_vim_set_status(ed, "file loaded with truncation");
    } else {
        ush_vim_set_status(ed, "file loaded");
    }

    return 1;
}

static int ush_vim_resolve_write_path(const ush_state *sh, const char *raw, char *out_path, u64 out_size) {
    if (out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_path[0] = '\0';
    if (raw == (const char *)0 || raw[0] == '\0') {
        return 0;
    }

    return ush_resolve_path(sh, raw, out_path, out_size);
}

static int ush_vim_run_command(ush_vim_editor *ed, const ush_state *sh) {
    const char *cmd = ed->command;
    char token0[USH_PATH_MAX];
    const char *rest = (const char *)0;
    char path[USH_PATH_MAX];

    if (ed == (ush_vim_editor *)0 || sh == (const ush_state *)0) {
        return 0;
    }

    ush_trim_line(ed->command);
    cmd = ed->command;

    if (cmd[0] == '\0') {
        return 1;
    }

    if (ush_streq(cmd, "q") != 0) {
        if (ed->modified != 0) {
            ush_vim_set_status(ed, "No write since last change (add ! to override)");
            return 0;
        }
        ed->quit_requested = 1;
        return 1;
    }

    if (ush_streq(cmd, "q!") != 0) {
        ed->quit_requested = 1;
        return 1;
    }

    if (ush_streq(cmd, "wq") != 0 || ush_streq(cmd, "x") != 0) {
        if (ed->file_path[0] == '\0') {
            ush_vim_set_status(ed, "No file name");
            return 0;
        }
        if (ush_vim_save_file(ed, ed->file_path) == 0) {
            return 0;
        }
        ed->quit_requested = 1;
        return 1;
    }

    if (ush_split_first_and_rest(cmd, token0, (u64)sizeof(token0), &rest) != 0 && ush_streq(token0, "w") != 0) {
        char write_target[USH_PATH_MAX];

        write_target[0] = '\0';

        if (rest == (const char *)0 || rest[0] == '\0') {
            if (ed->file_path[0] == '\0') {
                ush_vim_set_status(ed, "No file name");
                return 0;
            }
            ush_copy(write_target, (u64)sizeof(write_target), ed->file_path);
        } else {
            if (ush_vim_resolve_write_path(sh, rest, path, (u64)sizeof(path)) == 0) {
                ush_vim_set_status(ed, "invalid path");
                return 0;
            }
            ush_copy(write_target, (u64)sizeof(write_target), path);
        }

        return ush_vim_save_file(ed, write_target);
    }

    if (ush_streq(cmd, "help") != 0) {
        ush_vim_set_status(ed, "normal: h j k l i a o x dd gg G : | cmd: w q q! wq");
        return 1;
    }

    ush_vim_set_status(ed, "unknown command");
    return 0;
}

static void ush_vim_handle_insert_mode(ush_vim_editor *ed, int key) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (key == USH_VIM_KEY_ESC) {
        ed->mode = USH_VIM_MODE_NORMAL;
        ed->pending_dd = 0;
        ed->pending_gg = 0;
        ed->pending_jj = 0;
        ush_vim_set_status(ed, "-- NORMAL --");
        return;
    }

    if (ed->pending_jj != 0) {
        if (key == 'j') {
            ed->pending_jj = 0;
            ed->mode = USH_VIM_MODE_NORMAL;
            ed->pending_dd = 0;
            ed->pending_gg = 0;
            ush_vim_set_status(ed, "-- NORMAL -- (jj)");
            return;
        }

        (void)ush_vim_insert_char(ed, 'j');
        ed->pending_jj = 0;
    }

    if (key == 'j') {
        ed->pending_jj = 1;
        return;
    }

    if (key == USH_VIM_KEY_LEFT) {
        ush_vim_move_left(ed);
        return;
    }
    if (key == USH_VIM_KEY_RIGHT) {
        ush_vim_move_right(ed);
        return;
    }
    if (key == USH_VIM_KEY_UP) {
        ush_vim_move_up(ed);
        return;
    }
    if (key == USH_VIM_KEY_DOWN) {
        ush_vim_move_down(ed);
        return;
    }
    if (key == USH_VIM_KEY_HOME) {
        ed->cursor_col = 0ULL;
        return;
    }
    if (key == USH_VIM_KEY_END) {
        ed->cursor_col = ed->lines[ed->cursor_line].len;
        return;
    }

    if (key == USH_VIM_KEY_ENTER || key == '\n') {
        (void)ush_vim_split_line(ed);
        return;
    }

    if (key == USH_VIM_KEY_BACKSPACE || key == USH_VIM_KEY_DEL) {
        (void)ush_vim_backspace(ed);
        return;
    }

    if (key == '\t') {
        (void)ush_vim_insert_char(ed, ' ');
        (void)ush_vim_insert_char(ed, ' ');
        (void)ush_vim_insert_char(ed, ' ');
        (void)ush_vim_insert_char(ed, ' ');
        return;
    }

    if (key >= 32 && key <= 255) {
        char bytes[4];
        u64 byte_len = 1ULL;

        bytes[0] = (char)key;
        if (key >= 0x80) {
            byte_len = ush_vim_collect_utf8(key, bytes, (u64)sizeof(bytes));
        }

        (void)ush_vim_insert_bytes(ed, bytes, byte_len);
    }
}

static void ush_vim_handle_normal_mode(ush_vim_editor *ed, int key) {
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (ed->pending_dd != 0) {
        if (key == 'd') {
            (void)ush_vim_delete_line(ed);
            ush_vim_set_status(ed, "line deleted");
        }
        ed->pending_dd = 0;
        return;
    }

    if (ed->pending_gg != 0) {
        if (key == 'g') {
            ed->cursor_line = 0ULL;
            ed->cursor_col = 0ULL;
        }
        ed->pending_gg = 0;
        return;
    }

    if (key == USH_VIM_KEY_LEFT || key == 'h') {
        ush_vim_move_left(ed);
        return;
    }
    if (key == USH_VIM_KEY_RIGHT || key == 'l') {
        ush_vim_move_right(ed);
        return;
    }
    if (key == USH_VIM_KEY_UP || key == 'k') {
        ush_vim_move_up(ed);
        return;
    }
    if (key == USH_VIM_KEY_DOWN || key == 'j') {
        ush_vim_move_down(ed);
        return;
    }
    if (key == USH_VIM_KEY_HOME || key == '0') {
        ed->cursor_col = 0ULL;
        return;
    }
    if (key == USH_VIM_KEY_END || key == '$') {
        ed->cursor_col = ed->lines[ed->cursor_line].len;
        return;
    }

    if (key == 'G') {
        ed->cursor_line = ed->line_count - 1ULL;
        ush_vim_keep_cursor_valid(ed);
        return;
    }
    if (key == 'g') {
        ed->pending_gg = 1;
        return;
    }

    if (key == 'i') {
        ed->mode = USH_VIM_MODE_INSERT;
        ed->pending_jj = 0;
        ush_vim_set_status(ed, "-- INSERT --");
        return;
    }
    if (key == 'a') {
        if (ed->cursor_col < ed->lines[ed->cursor_line].len) {
            ed->cursor_col = ush_vim_utf8_next_boundary(ed->lines[ed->cursor_line].text,
                                                        ed->lines[ed->cursor_line].len, ed->cursor_col);
        }
        ed->mode = USH_VIM_MODE_INSERT;
        ed->pending_jj = 0;
        ush_vim_set_status(ed, "-- INSERT --");
        return;
    }
    if (key == 'o') {
        (void)ush_vim_open_below_insert(ed);
        ed->pending_jj = 0;
        ush_vim_set_status(ed, "-- INSERT --");
        return;
    }

    if (key == 'x') {
        (void)ush_vim_delete_char(ed);
        return;
    }

    if (key == 'd') {
        ed->pending_dd = 1;
        return;
    }

    if (key == ':') {
        ed->mode = USH_VIM_MODE_COMMAND;
        ed->command_len = 0ULL;
        ed->command[0] = '\0';
        return;
    }

    if (key == USH_VIM_KEY_ESC) {
        return;
    }
}

static void ush_vim_handle_command_mode(ush_vim_editor *ed, const ush_state *sh, int key) {
    if (ed == (ush_vim_editor *)0 || sh == (const ush_state *)0) {
        return;
    }

    if (key == USH_VIM_KEY_ESC) {
        ed->mode = USH_VIM_MODE_NORMAL;
        ed->command_len = 0ULL;
        ed->command[0] = '\0';
        return;
    }

    if (key == USH_VIM_KEY_BACKSPACE || key == USH_VIM_KEY_DEL) {
        if (ed->command_len > 0ULL) {
            ed->command_len--;
            ed->command[ed->command_len] = '\0';
        }
        return;
    }

    if (key == USH_VIM_KEY_ENTER || key == '\n') {
        (void)ush_vim_run_command(ed, sh);
        ed->mode = USH_VIM_MODE_NORMAL;
        ed->command_len = 0ULL;
        ed->command[0] = '\0';
        return;
    }

    if (key >= 32 && key <= 255) {
        char bytes[4];
        u64 byte_len = 1ULL;

        bytes[0] = (char)key;
        if (key >= 0x80) {
            byte_len = ush_vim_collect_utf8(key, bytes, (u64)sizeof(bytes));
        }

        if (ed->command_len + byte_len < USH_VIM_CMD_MAX) {
            (void)memcpy(ed->command + ed->command_len, bytes, (size_t)byte_len);
            ed->command_len += byte_len;
            ed->command[ed->command_len] = '\0';
        }
    }
}

static int ush_vim_parse_start_path(const ush_state *sh, const char *arg, char *out_path, u64 out_size) {
    char path_token[USH_PATH_MAX];
    const char *rest = (const char *)0;

    if (out_path == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_path[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, path_token, (u64)sizeof(path_token), &rest) == 0) {
        return 0;
    }
    (void)rest;

    if (ush_resolve_path(sh, path_token, out_path, out_size) == 0) {
        return 0;
    }

    return 1;
}

static int ush_cmd_vim(ush_state *sh, const char *arg) {
    static ush_vim_editor ed;
    char target_path[USH_PATH_MAX];

    if (sh == (ush_state *)0) {
        return 0;
    }

    ush_vim_init(&ed);

    if (ush_vim_parse_start_path(sh, arg, target_path, (u64)sizeof(target_path)) == 0) {
        ush_writeln_i18n("vim: invalid path", "vim: 无效路径");
        return 0;
    }

    if (target_path[0] != '\0') {
        if (ush_vim_load_file(&ed, target_path) == 0) {
            ush_writeln_i18n("vim: failed to open file", "vim: 打开文件失败");
            return 0;
        }
    }

    /* Clear screen once on entry, then do anchored redraws to avoid visible flicker. */
    ush_write("\x1B[2J\x1B[H");
    ush_vim_keep_cursor_valid(&ed);
    ush_vim_ensure_cursor_visible(&ed);
    ush_vim_render(&ed);

    for (;;) {
        int key;
        u64 old_cursor_line;
        u64 old_cursor_col;
        u64 old_scroll_top;
        u64 old_line_count;
        int old_mode;
        int old_modified;
        char old_status[USH_VIM_STATUS_MAX];
        u64 dirty_line;
        int header_dirty;
        int pos_dirty;
        int footer_dirty;
        int line_dirty;
        int full_dirty;
        int edit_key_dirty;

        old_cursor_line = ed.cursor_line;
        old_cursor_col = ed.cursor_col;
        old_scroll_top = ed.scroll_top;
        old_line_count = ed.line_count;
        old_mode = ed.mode;
        old_modified = ed.modified;
        ush_copy(old_status, (u64)sizeof(old_status), ed.status);

        key = ush_vim_read_key();
        if (ed.mode == USH_VIM_MODE_INSERT) {
            ush_vim_handle_insert_mode(&ed, key);
        } else if (ed.mode == USH_VIM_MODE_COMMAND) {
            ush_vim_handle_command_mode(&ed, sh, key);
        } else {
            ush_vim_handle_normal_mode(&ed, key);
        }

        if (ed.quit_requested != 0) {
            break;
        }

        ush_vim_keep_cursor_valid(&ed);
        ush_vim_ensure_cursor_visible(&ed);

        dirty_line = old_cursor_line;
        if (ed.cursor_line < dirty_line) {
            dirty_line = ed.cursor_line;
        }
        header_dirty = (old_mode != ed.mode || old_modified != ed.modified) ? 1 : 0;
        pos_dirty = (old_cursor_line != ed.cursor_line || old_cursor_col != ed.cursor_col ||
                     old_line_count != ed.line_count)
                        ? 1
                        : 0;
        footer_dirty = (old_mode != ed.mode || old_status[0] != ed.status[0] ||
                        strcmp(old_status, ed.status) != 0 || old_mode == USH_VIM_MODE_COMMAND ||
                        ed.mode == USH_VIM_MODE_COMMAND)
                           ? 1
                           : 0;
        edit_key_dirty = 0;
        if (old_mode == USH_VIM_MODE_INSERT &&
            (key == USH_VIM_KEY_BACKSPACE || key == USH_VIM_KEY_DEL || key == '\t' ||
             (key >= 32 && key <= 255))) {
            edit_key_dirty = 1;
        }
        if (old_mode == USH_VIM_MODE_NORMAL && (key == 'x')) {
            edit_key_dirty = 1;
        }
        line_dirty = (old_cursor_line != ed.cursor_line || old_cursor_col != ed.cursor_col ||
                      old_modified != ed.modified || edit_key_dirty != 0)
                         ? 1
                         : 0;
        full_dirty = (old_scroll_top != ed.scroll_top || old_line_count != ed.line_count ||
                      key == USH_VIM_KEY_ENTER || key == '\n')
                         ? 1
                         : 0;

        if (full_dirty != 0) {
            ush_vim_render(&ed);
        } else {
            ush_vim_render_partial(&ed, header_dirty, pos_dirty, footer_dirty, line_dirty, dirty_line);
            if (line_dirty != 0 && old_cursor_line != ed.cursor_line) {
                ush_vim_render_partial(&ed, 0, 0, 0, 1, ed.cursor_line);
            }
        }
    }

    ush_write("\x1B[0m\x1B[2J\x1B[H");
    ush_writeln_i18n("vim: bye", "vim: 再见");
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "vim") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_vim(&sh, arg);

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
