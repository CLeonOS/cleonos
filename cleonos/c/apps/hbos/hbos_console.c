#include "hbos.h"

static hbos_u32 hbos_console_strlen(const char *text) {
    hbos_u32 len = 0U;
    if (text == (const char *)0) {
        return 0U;
    }
    while (text[len] != '\0') {
        len++;
    }
    return len;
}

static int hbos_console_join_path(char *out, hbos_u32 out_size, const char *dir, const char *name, int add_ext) {
    hbos_u32 p = 0U;
    hbos_u32 i;

    if (out == (char *)0 || out_size == 0U || name == (const char *)0 || name[0] == '\0') {
        return 0;
    }
    out[0] = '\0';
    if (dir != (const char *)0 && dir[0] != '\0') {
        i = 0U;
        while (dir[i] != '\0' && p + 1U < out_size) {
            out[p++] = dir[i++];
        }
        if (p > 0U && out[p - 1U] != '/' && p + 1U < out_size) {
            out[p++] = '/';
        }
    }
    i = 0U;
    while (name[i] != '\0' && p + 1U < out_size) {
        out[p++] = name[i++];
    }
    if (add_ext == 1 && p + 4U < out_size) {
        out[p++] = '.';
        out[p++] = 'h';
        out[p++] = 'r';
        out[p++] = 'b';
    } else if (add_ext == 2 && p + 4U < out_size) {
        out[p++] = '.';
        out[p++] = 'H';
        out[p++] = 'R';
        out[p++] = 'B';
    }
    out[p] = '\0';
    return (p + 1U < out_size) ? 1 : 0;
}

static int hbos_console_has_hrb_suffix(const char *name) {
    hbos_u32 len = hbos_console_strlen(name);
    if (len < 4U) {
        return 0;
    }
    return hbos_streq_ci(name + len - 4U, ".hrb");
}

static int hbos_console_ends_with_dot(const char *name) {
    hbos_u32 len = hbos_console_strlen(name);
    return (len > 0U && name[len - 1U] == '.') ? 1 : 0;
}

static int hbos_resolve_hrb_path(const char *name, char *out_path, hbos_u32 out_size) {
    static const char *dirs[] = {
        "",
        "/system/hbos",
        "/shell/hbos",
        "/temp",
    };
    hbos_u32 i;
    int has_ext;

    if (name == (const char *)0 || name[0] == '\0' || out_path == (char *)0 || out_size == 0U) {
        return 0;
    }
    if (name[0] == '/') {
        if (hbos_console_join_path(out_path, out_size, "", name, 0) != 0 &&
            cleonos_sys_fs_stat_type(out_path) == 1ULL) {
            return 1;
        }
        out_path[0] = '\0';
        return 0;
    }
    has_ext = hbos_console_has_hrb_suffix(name);
    for (i = 0U; i < (hbos_u32)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        if (hbos_console_join_path(out_path, out_size, dirs[i], name, 0) != 0 &&
            cleonos_sys_fs_stat_type(out_path) == 1ULL) {
            return 1;
        }
        if (has_ext == 0 && hbos_console_join_path(out_path, out_size, dirs[i], name, 1) != 0 &&
            cleonos_sys_fs_stat_type(out_path) == 1ULL) {
            return 1;
        }
        if (has_ext == 0 && hbos_console_join_path(out_path, out_size, dirs[i], name, 2) != 0 &&
            cleonos_sys_fs_stat_type(out_path) == 1ULL) {
            return 1;
        }
    }
    out_path[0] = '\0';
    return 0;
}

static int hbos_try_run_real_hrb(hbos_state *state, const char *name, const char *args) {
    char path[160];
    if (hbos_resolve_hrb_path(name, path, (hbos_u32)sizeof(path)) == 0) {
        return 0;
    }
    (void)hbos_hrb_run_path(state, path, args);
    return 1;
}

static void hbos_cmd_dir(hbos_state *state) {
    hbos_u32 i;
    for (i = 0U; i < hbos_file_count(); i++) {
        const hbos_file *file = hbos_file_at(i);
        char line[HBOS_CONSOLE_COLS + 1U];
        char name83[16];
        hbos_u32 p = 0U;
        hbos_u32 j = 0U;
        hbos_u32 n = hbos_file_size_for_state(state, file);
        char rev[12];
        hbos_u32 rn = 0U;
        hbos_format_83_name(file, name83, (hbos_u32)sizeof(name83));
        while (name83[j] != '\0' && p + 1U < sizeof(line)) line[p++] = name83[j++];
        while (p < 15U) line[p++] = ' ';
        if (n == 0U) rev[rn++] = '0';
        while (n > 0U && rn < sizeof(rev)) { rev[rn++] = (char)('0' + (n % 10U)); n /= 10U; }
        while (rn > 0U && p + 1U < sizeof(line)) line[p++] = rev[--rn];
        line[p] = '\0';
        hbos_put_history(state, line, HBOS_COLOR_CONSOLE_TEXT);
    }
    hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
}

static void hbos_cmd_mem(hbos_state *state) {
    hbos_put_history(state, "total   64MB", HBOS_COLOR_CONSOLE_TEXT);
    hbos_put_history(state, "free    60MB (emulated Haribote memory arena)", HBOS_COLOR_CONSOLE_TEXT);
    hbos_put_history_fmt_u32(state, "kernel  HARIBOTE.SYS ", state->haribote_kernel_size, " bytes",
                             HBOS_COLOR_CONSOLE_DIM);
    hbos_put_history(state, "host    CLeonOS user process + Haribote kernel image", HBOS_COLOR_CONSOLE_DIM);
    hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
}

static void hbos_clear_console(hbos_state *state) {
    hbos_u32 i;
    state->history_count = 0U;
    for (i = 0U; i < HBOS_HISTORY_MAX; i++) {
        state->history[i][0] = '\0';
        state->history_color[i] = HBOS_COLOR_CONSOLE_TEXT;
    }
    state->dirty = 1;
}

static void hbos_copy_cmdline(char *dst, hbos_u32 dst_size, const char *src) {
    hbos_u32 i = 0U;
    hbos_u32 start = 0U;
    hbos_u32 end = 0U;
    int last_space = 0;

    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }

    while (src[start] == ' ' || src[start] == '\t' || src[start] == '\r' || src[start] == '\n') {
        start++;
    }
    end = start;
    while (src[end] != '\0') {
        end++;
    }
    while (end > start &&
           (src[end - 1U] == ' ' || src[end - 1U] == '\t' || src[end - 1U] == '\r' || src[end - 1U] == '\n')) {
        end--;
    }

    while (start < end && i + 1U < dst_size) {
        unsigned char ch = (unsigned char)src[start++];
        if (ch == '\t') {
            ch = ' ';
        }
        if (ch >= 32U && ch <= 126U) {
            if (ch == ' ') {
                if (i == 0U || last_space != 0) {
                    continue;
                }
                last_space = 1;
            } else {
                last_space = 0;
            }
            dst[i++] = (char)ch;
        }
    }
    if (i > 0U && dst[i - 1U] == ' ') {
        i--;
    }
    dst[i] = '\0';
}

static void hbos_cmd_token(const char *line, char *cmd, hbos_u32 cmd_size, const char **out_rest) {
    hbos_u32 i = 0U;
    hbos_u32 p = 0U;

    if (cmd != (char *)0 && cmd_size > 0U) {
        cmd[0] = '\0';
    }
    if (out_rest != (const char **)0) {
        *out_rest = "";
    }
    if (line == (const char *)0 || cmd == (char *)0 || cmd_size == 0U) {
        return;
    }

    while (line[i] == ' ') {
        i++;
    }
    while (line[i] != '\0' && line[i] != ' ' && p + 1U < cmd_size) {
        cmd[p++] = line[i++];
    }
    cmd[p] = '\0';
    while (line[i] == ' ') {
        i++;
    }
    if (out_rest != (const char **)0) {
        *out_rest = line + i;
    }
}

static int hbos_cmd_app(hbos_state *state, const char *cmdline) {
    char name[18];
    hbos_u32 i = 0U;
    const hbos_app *app;

    if (cmdline == (const char *)0 || cmdline[0] == '\0') {
        return 0;
    }

    while (i < 13U && cmdline[i] > ' ') {
        name[i] = cmdline[i];
        i++;
    }
    name[i] = '\0';

    if (name[0] == '\0') {
        return 0;
    }

    if (hbos_try_run_real_hrb(state, name, cmdline) != 0) {
        hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
        return 1;
    }

    app = hbos_find_app(name);
    if (app == (const hbos_app *)0 && hbos_console_has_hrb_suffix(name) == 0 && hbos_console_ends_with_dot(name) == 0) {
        char name_hrb[24];
        hbos_u32 p = 0U;
        hbos_u32 n = 0U;
        while (name[n] != '\0' && p + 1U < sizeof(name_hrb)) {
            name_hrb[p++] = name[n++];
        }
        if (p + 5U < sizeof(name_hrb)) {
            name_hrb[p++] = '.';
            name_hrb[p++] = 'H';
            name_hrb[p++] = 'R';
            name_hrb[p++] = 'B';
            name_hrb[p] = '\0';
            if (hbos_try_run_real_hrb(state, name_hrb, cmdline) != 0) {
                hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
                return 1;
            }
            app = hbos_find_app(name_hrb);
        }
    }

    if (app == (const hbos_app *)0) {
        return 0;
    }

    (void)hbos_run_builtin_app(state, app, cmdline);
    hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
    return 1;
}

void hbos_execute_line(hbos_state *state, const char *line) {
    char cmdline[HBOS_CMD_MAX];
    char cmd[24];
    const char *rest;

    hbos_copy_cmdline(cmdline, (hbos_u32)sizeof(cmdline), line);
    hbos_cmd_token(cmdline, cmd, (hbos_u32)sizeof(cmd), &rest);

    if (cmd[0] == '\0') {
        return;
    }

    if (hbos_streq_ci(cmd, "exit2cleonos") != 0 && rest[0] == '\0') {
        hbos_put_history(state, "Leaving HariboteOS emulator, returning to CLeonOS...", HBOS_COLOR_OK);
        state->running = 0;
    } else if (hbos_streq_ci(cmd, "mem") != 0 && rest[0] == '\0') {
        hbos_cmd_mem(state);
    } else if (hbos_streq_ci(cmd, "cls") != 0 && rest[0] == '\0') {
        hbos_clear_console(state);
    } else if (hbos_streq_ci(cmd, "dir") != 0 && rest[0] == '\0') {
        hbos_cmd_dir(state);
    } else if (hbos_streq_ci(cmd, "exit") != 0 && rest[0] == '\0') {
        hbos_put_history(state, "Cannot close the host console; use exit2cleonos to return to CLeonOS.",
                         HBOS_COLOR_WARN);
        hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
    } else if (hbos_streq_ci(cmd, "start") != 0 && rest[0] != '\0') {
        hbos_put_history(state, "start: secondary Haribote console is not available in terminal-only host.",
                         HBOS_COLOR_WARN);
        hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
    } else if (hbos_streq_ci(cmd, "ncst") != 0 && rest[0] != '\0') {
        hbos_put_history(state, "ncst: no-window console task is not available in terminal-only host.",
                         HBOS_COLOR_WARN);
        hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
    } else if (hbos_streq_ci(cmd, "langmode") != 0 && rest[0] != '\0') {
        hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
    } else {
        if (hbos_cmd_app(state, cmdline) == 0) {
            hbos_put_history(state, "Bad command.", HBOS_COLOR_WARN);
            hbos_put_history(state, "", HBOS_COLOR_CONSOLE_TEXT);
        }
    }
}

void hbos_console_prompt(hbos_state *state) {
    state->input[0] = '\0';
    state->input_len = 0U;
    state->dirty = 1;
    if (state->terminal_only != 0) {
        state->prompt_pending = 1;
    }
}

void hbos_console_backspace(hbos_state *state) {
    if (state->input_len > 0U) {
        state->input_len--;
        state->input[state->input_len] = '\0';
        state->dirty = 1;
        if (state->terminal_only != 0) {
            hbos_terminal_write("\b \b");
        }
    }
}

void hbos_console_input_char(hbos_state *state, char ch) {
    if ((unsigned char)ch < 32U || ch == 127) {
        return;
    }
    if (state->input_len + 1U < HBOS_CMD_MAX) {
        state->input[state->input_len++] = ch;
        state->input[state->input_len] = '\0';
        state->dirty = 1;
        if (state->terminal_only != 0) {
            char echo[2];
            echo[0] = ch;
            echo[1] = '\0';
            hbos_terminal_write(echo);
        }
    }
}

void hbos_console_submit(hbos_state *state) {
    char echo[HBOS_CONSOLE_COLS + 1U];
    hbos_u32 p = 0U;
    hbos_u32 i = 0U;

    if (state != (hbos_state *)0 && state->terminal_only != 0) {
        hbos_terminal_write("\n");
        hbos_execute_line(state, state->input);
        hbos_console_prompt(state);
        return;
    }

    echo[p++] = '>';
    echo[p++] = ' ';
    while (state->input[i] != '\0' && p + 1U < sizeof(echo)) {
        echo[p++] = state->input[i++];
    }
    echo[p] = '\0';
    hbos_put_history(state, echo, HBOS_COLOR_OK);
    hbos_execute_line(state, state->input);
    hbos_console_prompt(state);
}
