#include "cmd_runtime.h"

static int ush_ps_is_user_path(const char *path) {
    if (path == (const char *)0 || path[0] != '/') {
        return 0;
    }

    if (path[1] == 's' && path[2] == 'y' && path[3] == 's' && path[4] == 't' && path[5] == 'e' && path[6] == 'm' &&
        (path[7] == '/' || path[7] == '\0')) {
        return 0;
    }

    if (path[1] == 'd' && path[2] == 'r' && path[3] == 'i' && path[4] == 'v' && path[5] == 'e' && path[6] == 'r' &&
        (path[7] == '/' || path[7] == '\0')) {
        return 0;
    }

    return 1;
}

static int ush_ps_next_token(const char **io_cursor, char *out, u64 out_size) {
    const char *p;
    u64 n = 0ULL;

    if (io_cursor == (const char **)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }

    out[0] = '\0';
    p = *io_cursor;

    if (p == (const char *)0) {
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) != 0) {
        p++;
    }

    if (*p == '\0') {
        *io_cursor = p;
        return 0;
    }

    while (*p != '\0' && ush_is_space(*p) == 0) {
        if (n + 1ULL < out_size) {
            out[n++] = *p;
        }
        p++;
    }

    out[n] = '\0';
    *io_cursor = p;
    return 1;
}

static void ush_ps_print_one(const cleonos_proc_snapshot *snap) {
    if (snap == (const cleonos_proc_snapshot *)0) {
        return;
    }

    ush_write("pid=");
    ush_write_u64_dec(snap->pid);
    ush_write(" parent=");
    ush_write_u64_dec(snap->ppid);
    ush_write(" state=");
    ush_write(ush_proc_state_name(snap->state));
    if (snap->blocked_reason != CLEONOS_BLOCK_NONE) {
        ush_write(" block=\"");
        ush_write(ush_block_reason_name(snap->blocked_reason));
        ush_write("\"");
    }
    ush_write(" tty=");
    ush_write_u64_dec(snap->tty_index);
    ush_write(" runtime=");
    ush_write_u64_dec(snap->runtime_ticks);
    ush_write(" ticks mem=");
    ush_write_human_bytes(snap->mem_bytes);
    ush_write(" path=");
    ush_write(snap->path);
    if (snap->state == CLEONOS_PROC_STATE_EXITED) {
        ush_write(" ");
        ush_print_exit_status_i18n("exit", "退出", snap->exit_status);
    } else {
        ush_write_char('\n');
    }
}

static int ush_cmd_ps(const char *arg) {
    u64 proc_count;
    u64 i;
    u64 shown = 0ULL;
    int include_exited = 0;
    int only_user = 0;
    const char *cursor = arg;
    char token[USH_PATH_MAX];

    while (ush_ps_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "-a") != 0 || ush_streq(token, "--all") != 0) {
            include_exited = 1;
        } else if (ush_streq(token, "-u") != 0 || ush_streq(token, "--user") != 0) {
            only_user = 1;
        } else {
            ush_writeln_i18n("ps: usage ps [-a|--all] [-u|--user]", "ps: 用法 ps [-a|--all] [-u|--user]");
            return 0;
        }
    }

    proc_count = cleonos_sys_proc_count();
    ush_writeln_i18n("processes:", "进程:");

    for (i = 0ULL; i < proc_count; i++) {
        u64 pid = 0ULL;
        cleonos_proc_snapshot snap;

        if (cleonos_sys_proc_pid_at(i, &pid) == 0ULL || pid == 0ULL) {
            continue;
        }

        if (cleonos_sys_proc_snapshot(pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            continue;
        }

        if (include_exited == 0 && snap.state == CLEONOS_PROC_STATE_EXITED) {
            continue;
        }

        if (only_user != 0 && ush_ps_is_user_path(snap.path) == 0) {
            continue;
        }

        ush_ps_print_one(&snap);
        shown++;
    }

    if (shown == 0ULL) {
        ush_writeln_i18n("(no process)", "(没有进程)");
    }

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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ps") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ps(arg);

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
