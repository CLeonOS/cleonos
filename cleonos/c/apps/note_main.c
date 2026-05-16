#include "user_cmd_common.h"
#include <stdio.h>

#define NOTE_LINE_MAX 512U

static void note_default_path(char *out_path, u64 out_size) {
    cleonos_user_info info;

    if (out_path == (char *)0 || out_size == 0ULL) {
        return;
    }

    if (cleonos_sys_user_current(&info) != 0ULL && info.logged_in != 0ULL && info.home[0] != '\0') {
        (void)snprintf(out_path, (size_t)out_size, "%s/note.txt", info.home);
        return;
    }

    ush_copy(out_path, out_size, "/temp/note.txt");
}

static int note_show_file(const char *path) {
    char buf[NOTE_LINE_MAX];
    u64 fd;

    if (path == (const char *)0 || path[0] == '\0') {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        ush_writeln_i18n("note: empty", "note: 为空");
        return 1;
    }

    for (;;) {
        u64 got = cleonos_sys_fd_read(fd, buf, (u64)sizeof(buf));
        u64 written = 0ULL;

        if (got == (u64)-1) {
            (void)cleonos_sys_fd_close(fd);
            ush_writeln_i18n("note: read failed", "note: 读取失败");
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        buf[got < (u64)sizeof(buf) ? got : ((u64)sizeof(buf) - 1ULL)] = '\0';
        while (written < got) {
            u64 out = cleonos_sys_fd_write(1ULL, buf + written, got - written);
            if (out == 0ULL || out == (u64)-1) {
                (void)cleonos_sys_fd_close(fd);
                ush_writeln_i18n("note: write failed", "note: 写入失败");
                return 0;
            }
            written += out;
        }
    }

    (void)cleonos_sys_fd_close(fd);
    return 1;
}

static int note_write_text(const char *path, const char *text, int append) {
    u64 len;

    if (path == (const char *)0 || text == (const char *)0) {
        return 0;
    }

    len = ush_strlen(text);
    if (append != 0) {
        if (cleonos_sys_fs_append(path, text, len) == 0ULL) {
            return 0;
        }
        if (len == 0ULL || text[len - 1ULL] != '\n') {
            if (cleonos_sys_fs_append(path, "\n", 1ULL) == 0ULL) {
                return 0;
            }
        }
        return 1;
    }

    return (cleonos_sys_fs_write(path, text, len) != 0ULL) ? 1 : 0;
}

static int note_prompt_multiline(char *out_line, u64 out_size) {
    if (out_line == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out_line[0] = '\0';
    ush_write_i18n_label("line", "行");
    ush_write(": ");
    return (ucmd_read_plain_line("", out_line, out_size) != 0) ? 1 : 0;
}

static int note_edit_interactive(const char *path) {
    char line[NOTE_LINE_MAX];

    if (path == (const char *)0) {
        return 0;
    }

    if (cleonos_sys_fs_write(path, "", 0ULL) == 0ULL) {
        return 0;
    }

    ush_writeln_i18n("note: enter lines, blank line or single '.' to finish",
                     "note: 逐行输入，空行或单独一个 '.' 结束");
    for (;;) {
        line[0] = '\0';
        if (note_prompt_multiline(line, (u64)sizeof(line)) == 0) {
            break;
        }
        if (line[0] == '\0' || (line[0] == '.' && line[1] == '\0')) {
            break;
        }
        if (note_write_text(path, line, 1) == 0) {
            ush_writeln_i18n("note: write failed", "note: 写入失败");
            return 0;
        }
    }

    return 1;
}

static int note_handle_action(const char *path, const char *cmd, const char *rest) {
    if (cmd == (const char *)0 || cmd[0] == '\0' || ush_streq(cmd, "show") != 0 || ush_streq(cmd, "view") != 0) {
        return note_show_file(path);
    }

    if (ush_streq(cmd, "path") != 0) {
        ush_writeln(path);
        return 1;
    }

    if (ush_streq(cmd, "clear") != 0) {
        return (cleonos_sys_fs_write(path, "", 0ULL) != 0ULL) ? 1 : 0;
    }

    if (ush_streq(cmd, "edit") != 0) {
        return note_edit_interactive(path);
    }

    if (ush_streq(cmd, "add") != 0 || ush_streq(cmd, "set") != 0) {
        const char *payload = rest;
        char tmp[NOTE_LINE_MAX];
        int append = (ush_streq(cmd, "add") != 0) ? 1 : 0;

        if (payload == (const char *)0 || payload[0] == '\0') {
            if (ush_pipeline_stdin_text != (const char *)0 && ush_pipeline_stdin_text[0] != '\0') {
                payload = ush_pipeline_stdin_text;
            } else {
                ush_write_i18n_label("text", "文本");
                ush_write(": ");
                if (ucmd_read_plain_line("", tmp, (u64)sizeof(tmp)) == 0) {
                    return 0;
                }
                payload = tmp;
            }
        }

        return note_write_text(path, payload, append);
    }

    ush_writeln_i18n("note: usage note [show|view|path|clear|add <text>|set <text>|edit]",
                     "note: 用法 note [show|view|path|clear|add <文本>|set <文本>|edit]");
    return 0;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char path[USH_PATH_MAX];
    char arg[USH_ARG_MAX];
    char cmd[32];
    char rest[USH_ARG_MAX];
    int has_context = 0;
    int success = 0;
    const char *payload = (const char *)0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(arg, (u64)sizeof(arg));
    ush_zero(cmd, (u64)sizeof(cmd));
    ush_zero(rest, (u64)sizeof(rest));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
    note_default_path(path, (u64)sizeof(path));

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "note") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
            ush_copy(arg, (u64)sizeof(arg), ctx.arg);
        }
    }

    if (arg[0] != '\0') {
        if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &payload) == 0) {
            cmd[0] = '\0';
        } else if (payload != (const char *)0 && payload[0] != '\0') {
            ush_copy(rest, (u64)sizeof(rest), payload);
        }
    }

    success = note_handle_action(path, cmd, rest[0] != '\0' ? rest : payload);

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
