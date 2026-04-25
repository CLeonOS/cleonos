#include "cmd_runtime.h"

#define USH_VIM_MAX_LINES 2048ULL
#define USH_VIM_MAX_LINE_LEN 512ULL
#define USH_VIM_STATUS_MAX 160ULL
#define USH_VIM_CMD_MAX 128ULL
#define USH_VIM_VIEW_ROWS 20ULL
#define USH_VIM_VIEW_COLS 72ULL
#define USH_VIM_UI_COLS (5ULL + USH_VIM_VIEW_COLS)

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
    if (ed == (ush_vim_editor *)0) {
        return;
    }

    if (ed->cursor_col > 0ULL) {
        ed->cursor_col--;
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
        ed->cursor_col++;
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

static int ush_vim_insert_char(ush_vim_editor *ed, char ch) {
    ush_vim_line *line;

    if (ed == (ush_vim_editor *)0) {
        return 0;
    }

    if ((unsigned char)ch < 0x20U || ch == '\n' || ch == '\r') {
        return 0;
    }

    line = &ed->lines[ed->cursor_line];
    if (line->len + 1ULL >= USH_VIM_MAX_LINE_LEN) {
        ush_vim_set_status(ed, "line too long");
        return 0;
    }

    if (ed->cursor_col < line->len) {
        (void)memmove(line->text + ed->cursor_col + 1ULL, line->text + ed->cursor_col, (size_t)(line->len - ed->cursor_col));
    }
    line->text[ed->cursor_col] = ch;
    line->len++;
    line->text[line->len] = '\0';
    ed->cursor_col++;
    ed->modified = 1;
    return 1;
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
        if (ed->cursor_col < line->len) {
            (void)memmove(line->text + ed->cursor_col - 1ULL, line->text + ed->cursor_col, (size_t)(line->len - ed->cursor_col));
        }
        line->len--;
        line->text[line->len] = '\0';
        ed->cursor_col--;
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

    if (ed->cursor_col + 1ULL < line->len) {
        (void)memmove(line->text + ed->cursor_col, line->text + ed->cursor_col + 1ULL, (size_t)(line->len - ed->cursor_col - 1ULL));
    }
    line->len--;
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

static void ush_vim_write_padded_line(const char *text) {
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

    ush_write_char('\n');
}

static void ush_vim_write_separator(void) {
    u64 i;

    for (i = 0ULL; i < USH_VIM_UI_COLS; i++) {
        ush_write_char('-');
    }
    ush_write_char('\n');
}

static void ush_vim_render_line(const ush_vim_editor *ed, u64 line_index) {
    char buf[256];
    u64 col;
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
        ush_write_char('\n');
        return;
    }

    line = &ed->lines[line_index];
    visual_line_no = line_index + 1ULL;
    (void)snprintf(buf, sizeof(buf), "%4llu ", (unsigned long long)visual_line_no);
    ush_write(buf);

    for (col = 0ULL; col < USH_VIM_VIEW_COLS; col++) {
        char out_ch = ' ';
        if (col < line->len) {
            out_ch = line->text[col];
        }

        if (line_index == ed->cursor_line && col == ed->cursor_col) {
            ush_write_char(out_ch);
        } else {
            ush_write_char(out_ch);
        }
    }

    ush_write_char('\n');
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
        row = 5ULL + USH_VIM_VIEW_ROWS;
        col = 2ULL + ed->command_len;
        if (col > USH_VIM_UI_COLS) {
            col = USH_VIM_UI_COLS;
        }
    } else {
        u64 visual_row = 0ULL;
        u64 visual_col = ed->cursor_col;

        if (ed->cursor_line > ed->scroll_top) {
            visual_row = ed->cursor_line - ed->scroll_top;
        }
        if (visual_row >= USH_VIM_VIEW_ROWS) {
            visual_row = USH_VIM_VIEW_ROWS - 1ULL;
        }
        if (visual_col >= USH_VIM_VIEW_COLS) {
            visual_col = USH_VIM_VIEW_COLS - 1ULL;
        }

        row = 4ULL + visual_row;
        col = 6ULL + visual_col;
    }

    (void)snprintf(seq, sizeof(seq), "\x1B[%llu;%lluH\x1B[?25h", (unsigned long long)row, (unsigned long long)col);
    ush_write(seq);
}

static void ush_vim_render(const ush_vim_editor *ed) {
    char header[256];
    char pos[128];
    u64 row;
    const char *name;

    if (ed == (const ush_vim_editor *)0) {
        return;
    }

    name = (ed->file_path[0] != '\0') ? ed->file_path : "[No Name]";

    ush_write("\x1B[H");
    (void)snprintf(header, sizeof(header), "vim clone | %s | %s%s", name, ush_vim_mode_text(ed),
                   (ed->modified != 0) ? " [+]" : "");
    ush_vim_write_padded_line(header);

    (void)snprintf(pos, sizeof(pos), "line %llu/%llu col %llu",
                   (unsigned long long)(ed->cursor_line + 1ULL), (unsigned long long)ed->line_count,
                   (unsigned long long)(ed->cursor_col + 1ULL));
    ush_vim_write_padded_line(pos);
    ush_vim_write_separator();

    for (row = 0ULL; row < USH_VIM_VIEW_ROWS; row++) {
        ush_vim_render_line(ed, ed->scroll_top + row);
    }

    ush_vim_write_separator();
    if (ed->mode == USH_VIM_MODE_COMMAND) {
        char cmdline[USH_VIM_CMD_MAX + 2ULL];

        cmdline[0] = ':';
        cmdline[1] = '\0';
        ush_copy(cmdline + 1ULL, (u64)sizeof(cmdline) - 1ULL, ed->command);
        ush_vim_write_padded_line(cmdline);
    } else {
        ush_vim_write_padded_line(ed->status);
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

    if (key >= 32 && key <= 126) {
        (void)ush_vim_insert_char(ed, (char)key);
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
            ed->cursor_col++;
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

    if (key >= 32 && key <= 126) {
        if (ed->command_len + 1ULL < USH_VIM_CMD_MAX) {
            ed->command[ed->command_len++] = (char)key;
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
        ush_writeln("vim: invalid path");
        return 0;
    }

    if (target_path[0] != '\0') {
        if (ush_vim_load_file(&ed, target_path) == 0) {
            ush_writeln("vim: failed to open file");
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
        ush_vim_render(&ed);
    }

    ush_write("\x1B[0m\x1B[2J\x1B[H");
    ush_writeln("vim: bye");
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
