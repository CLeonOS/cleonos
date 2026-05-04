#include "cmd_runtime.h"
static int ush_cmd_wait(const char *arg) {
    u64 pid;
    u64 status = (u64)-1;
    u64 wait_ret;

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln_i18n("wait: usage wait <pid>", "wait: 用法 wait <pid>");
        return 0;
    }

    if (ush_parse_u64_dec(arg, &pid) == 0) {
        ush_writeln_i18n("wait: invalid pid", "wait: 无效进程号");
        return 0;
    }

    wait_ret = cleonos_sys_wait_pid(pid, &status);

    if (wait_ret == (u64)-1) {
        ush_writeln_i18n("wait: pid not found", "wait: 找不到进程");
        return 0;
    }

    if (wait_ret == 0ULL) {
        ush_writeln_i18n("wait: still running", "wait: 仍在运行");
        return 1;
    }

    ush_writeln_i18n("wait: exited", "wait: 已退出");
    if ((status & (1ULL << 63)) != 0ULL) {
        ush_print_kv_hex_i18n("  SIGNAL", "  信号", status & 0xFFULL);
        ush_print_kv_hex_i18n("  VECTOR", "  向量", (status >> 8) & 0xFFULL);
        ush_print_kv_hex_i18n("  ERROR", "  错误码", (status >> 16) & 0xFFFFULL);
    } else {
        ush_print_kv_hex_i18n("  STATUS", "  状态", status);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "wait") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_wait(arg);

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
