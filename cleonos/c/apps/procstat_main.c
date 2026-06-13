#include "cmd_runtime.h"

static int ush_procstat_next_token(const char **io_cursor, char *out, u64 out_size) {
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

static void ush_procstat_print_line(const cleonos_proc_snapshot *snap) {
    if (snap == (const cleonos_proc_snapshot *)0) {
        return;
    }

    ush_write("pid=");
    ush_write_u64_dec(snap->pid);
    ush_write(" state=");
    ush_write(ush_proc_state_name(snap->state));
    ush_write(" thread=");
    ush_write(ush_thread_state_name(snap->thread_state));
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
    if (snap->last_signal != 0ULL) {
        ush_write(" last_signal=");
        ush_write(ush_signal_name(snap->last_signal));
        ush_write("(");
        ush_write_u64_dec(snap->last_signal);
        ush_write(")");
    }
    ush_write(" path=");
    ush_writeln(snap->path);
}

static void ush_procstat_print_detail(const cleonos_proc_snapshot *snap) {
    if (snap == (const cleonos_proc_snapshot *)0) {
        return;
    }

    ush_writeln_i18n("process detail:", "进程详情:");
    ush_print_kv_dec_i18n("  pid", "  进程号", snap->pid);
    ush_print_kv_dec_i18n("  parent pid", "  父进程号", snap->ppid);
    ush_write_i18n_label("  process state", "  进程状态");
    ush_write(": ");
    ush_write(ush_proc_state_name(snap->state));
    ush_write_char('\n');
    ush_write_i18n_label("  thread state", "  线程状态");
    ush_write(": ");
    ush_write(ush_thread_state_name(snap->thread_state));
    ush_write_char('\n');
    ush_write_i18n_label("  blocked reason", "  阻塞原因");
    ush_write(": ");
    ush_write(ush_block_reason_name(snap->blocked_reason));
    ush_write_char('\n');
    ush_print_kv_dec_i18n("  main thread", "  主线程", snap->main_thread_id);
    ush_print_kv_dec_i18n("  scheduler task", "  调度任务", snap->scheduler_task_id);
    ush_print_kv_dec_i18n("  wake tick", "  唤醒Tick", snap->wake_tick);
    ush_print_kv_dec_i18n("  waiting for pid", "  等待进程号", snap->wait_target_pid);
    ush_print_kv_dec_i18n("  parent waiting", "  父进程等待中", snap->parent_waiting);
    ush_print_kv_dec_i18n("  tty", "  终端", snap->tty_index);
    ush_print_kv_dec_i18n("  started tick", "  启动Tick", snap->started_tick);
    ush_print_kv_dec_i18n("  exited tick", "  退出Tick", snap->exited_tick);
    ush_print_kv_dec_i18n("  runtime ticks", "  运行Ticks", snap->runtime_ticks);
    ush_print_kv_bytes_i18n("  memory", "  内存", snap->mem_bytes);
    ush_print_exit_status_i18n("  exit status", "  退出状态", snap->exit_status);
    ush_write_i18n_label("  last signal", "  最后信号");
    ush_write(": ");
    ush_write(ush_signal_name(snap->last_signal));
    ush_write(" (");
    ush_write_u64_dec(snap->last_signal);
    ush_write(")\n");
    ush_print_kv_dec_i18n("  last fault vector", "  最后异常向量", snap->last_fault_vector);
    ush_print_kv_dec_i18n("  last fault error", "  最后异常错误码", snap->last_fault_error);
    ush_print_kv_hex_i18n("  LAST_FAULT_RIP", "  最后异常RIP", snap->last_fault_rip);
    ush_write_i18n_label("  path", "  路径");
    ush_write(": ");
    ush_writeln(snap->path);
}

static int ush_procstat_parse_args(const char *arg, u64 *out_pid, int *out_has_pid, int *out_include_exited) {
    const char *cursor = arg;
    char token[USH_PATH_MAX];
    u64 parsed_pid = 0ULL;
    int has_pid = 0;
    int include_exited = 0;

    if (out_pid == (u64 *)0 || out_has_pid == (int *)0 || out_include_exited == (int *)0) {
        return 0;
    }

    while (ush_procstat_next_token(&cursor, token, (u64)sizeof(token)) != 0) {
        if (ush_streq(token, "-a") != 0 || ush_streq(token, "--all") != 0) {
            include_exited = 1;
            continue;
        }

        if (ush_streq(token, "self") != 0) {
            if (has_pid != 0) {
                return 0;
            }
            parsed_pid = cleonos_sys_getpid();
            has_pid = 1;
            continue;
        }

        if (ush_parse_u64_dec(token, &parsed_pid) != 0 && parsed_pid != 0ULL) {
            if (has_pid != 0) {
                return 0;
            }
            has_pid = 1;
            continue;
        }

        return 0;
    }

    *out_pid = parsed_pid;
    *out_has_pid = has_pid;
    *out_include_exited = include_exited;
    return 1;
}

static int ush_cmd_procstat(const char *arg) {
    u64 target_pid = 0ULL;
    int has_pid = 0;
    int include_exited = 0;

    if (ush_procstat_parse_args(arg, &target_pid, &has_pid, &include_exited) == 0) {
        ush_writeln_i18n("procstat: usage procstat [pid|self] [-a|--all]",
                         "procstat: 用法 procstat [pid|self] [-a|--all]");
        return 0;
    }

    if (has_pid != 0) {
        cleonos_proc_snapshot snap;

        if (cleonos_sys_proc_snapshot(target_pid, &snap, (u64)sizeof(snap)) == 0ULL) {
            ush_writeln_i18n("procstat: pid not found", "procstat: 找不到进程");
            return 0;
        }

        ush_procstat_print_detail(&snap);
        return 1;
    }

    {
        u64 proc_count = cleonos_sys_proc_count();
        u64 i;
        u64 shown = 0ULL;

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

            ush_procstat_print_line(&snap);
            shown++;
        }

        if (shown == 0ULL) {
            ush_writeln_i18n("(no process)", "(没有进程)");
        }
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "procstat") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_procstat(arg);

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
