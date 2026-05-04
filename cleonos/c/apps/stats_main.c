#include "cmd_runtime.h"
static int ush_cmd_memstat(void) {
    ush_writeln_i18n("memstat (user ABI limited):", "内存/服务统计 (memstat, user ABI limited):");
    ush_print_kv_hex_i18n("  SERVICE_COUNT", "  服务数", cleonos_sys_service_count());
    ush_print_kv_hex_i18n("  SERVICE_READY_COUNT", "  就绪服务数", cleonos_sys_service_ready_count());
    ush_print_kv_hex_i18n("  KELF_COUNT", "  KELF 数量", cleonos_sys_kelf_count());
    ush_print_kv_hex_i18n("  KELF_RUNS", "  KELF 运行次数", cleonos_sys_kelf_runs());
    return 1;
}

static int ush_cmd_fsstat(void) {
    ush_writeln_i18n("fsstat:", "文件系统统计 (fsstat):");
    ush_print_kv_hex_i18n("  NODE_COUNT", "  节点数", cleonos_sys_fs_node_count());
    ush_print_kv_hex_i18n("  ROOT_CHILDREN", "  根目录子项", cleonos_sys_fs_child_count("/"));
    ush_print_kv_hex_i18n("  SYSTEM_CHILDREN", "  系统目录子项", cleonos_sys_fs_child_count("/system"));
    ush_print_kv_hex_i18n("  SHELL_CHILDREN", "  外壳目录子项", cleonos_sys_fs_child_count("/shell"));
    ush_print_kv_hex_i18n("  TEMP_CHILDREN", "  临时目录子项", cleonos_sys_fs_child_count("/temp"));
    ush_print_kv_hex_i18n("  DRIVER_CHILDREN", "  驱动目录子项", cleonos_sys_fs_child_count("/driver"));
    ush_print_kv_hex_i18n("  DEV_CHILDREN", "  设备目录子项", cleonos_sys_fs_child_count("/dev"));
    return 1;
}

static int ush_cmd_taskstat(void) {
    ush_writeln_i18n("taskstat:", "任务统计 (taskstat):");
    ush_print_kv_hex_i18n("  TASK_COUNT", "  任务数", cleonos_sys_task_count());
    ush_print_kv_hex_i18n("  CURRENT_TASK", "  当前任务", cleonos_syscall(CLEONOS_SYSCALL_CUR_TASK, 0ULL, 0ULL, 0ULL));
    ush_print_kv_hex_i18n("  TIMER_TICKS", "  计时器滴答", cleonos_sys_timer_ticks());
    ush_print_kv_hex_i18n("  CONTEXT_SWITCHES", "  上下文切换", cleonos_sys_context_switches());
    return 1;
}

static int ush_cmd_userstat(void) {
    ush_writeln_i18n("userstat:", "用户态统计 (userstat):");
    ush_print_kv_hex_i18n("  USER_SHELL_READY", "  用户外壳就绪", cleonos_sys_user_shell_ready());
    ush_print_kv_hex_i18n("  USER_EXEC_REQUESTED", "  用户执行请求", cleonos_sys_user_exec_requested());
    ush_print_kv_hex_i18n("  USER_LAUNCH_TRIES", "  用户启动尝试", cleonos_sys_user_launch_tries());
    ush_print_kv_hex_i18n("  USER_LAUNCH_OK", "  用户启动成功", cleonos_sys_user_launch_ok());
    ush_print_kv_hex_i18n("  USER_LAUNCH_FAIL", "  用户启动失败", cleonos_sys_user_launch_fail());
    ush_print_kv_hex_i18n("  EXEC_REQUESTS", "  执行请求", cleonos_sys_exec_request_count());
    ush_print_kv_hex_i18n("  EXEC_SUCCESS", "  执行成功", cleonos_sys_exec_success_count());
    ush_print_kv_hex_i18n("  TTY_COUNT", "  TTY 数量", cleonos_sys_tty_count());
    ush_print_kv_hex_i18n("  TTY_ACTIVE", "  当前 TTY", cleonos_sys_tty_active());
    return 1;
}

static int ush_cmd_kbdstat(void) {
    ush_writeln_i18n("kbdstat:", "键盘统计 (kbdstat):");
    ush_print_kv_hex_i18n("  BUFFERED", "  缓冲数量", cleonos_sys_kbd_buffered());
    ush_print_kv_hex_i18n("  PUSHED", "  推入次数", cleonos_sys_kbd_pushed());
    ush_print_kv_hex_i18n("  POPPED", "  弹出次数", cleonos_sys_kbd_popped());
    ush_print_kv_hex_i18n("  DROPPED", "  丢弃次数", cleonos_sys_kbd_dropped());
    ush_print_kv_hex_i18n("  HOTKEY_SWITCHES", "  热键切换次数", cleonos_sys_kbd_hotkey_switches());
    return 1;
}

static int ush_cmd_shstat(const ush_state *sh) {
    ush_writeln_i18n("shstat:", "外壳统计 (shstat):");
    ush_print_kv_hex_i18n("  CMD_TOTAL", "  命令总数", sh->cmd_total);
    ush_print_kv_hex_i18n("  CMD_OK", "  命令成功", sh->cmd_ok);
    ush_print_kv_hex_i18n("  CMD_FAIL", "  命令失败", sh->cmd_fail);
    ush_print_kv_hex_i18n("  CMD_UNKNOWN", "  未知命令", sh->cmd_unknown);
    ush_print_kv_hex_i18n("  EXIT_REQUESTED", "  请求退出", (sh->exit_requested != 0) ? 1ULL : 0ULL);
    ush_print_kv_hex_i18n("  EXIT_CODE", "  退出码", sh->exit_code);
    return 1;
}

static int ush_cmd_stats(const ush_state *sh) {
    (void)ush_cmd_memstat();
    (void)ush_cmd_fsstat();
    (void)ush_cmd_taskstat();
    (void)ush_cmd_userstat();
    (void)ush_cmd_kbdstat();
    (void)ush_cmd_shstat(sh);
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "stats") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_stats(&sh);

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
