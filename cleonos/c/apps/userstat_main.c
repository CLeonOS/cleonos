#include "cmd_runtime.h"
static int ush_cmd_userstat(void) {
    ush_writeln_i18n("userstat:", "用户态统计 (userstat):");
    ush_print_kv_hex_i18n("  USER_SHELL_READY", "  用户外壳就绪",
                          cleonos_sys_user_shell_ready());
    ush_print_kv_hex_i18n("  USER_EXEC_REQUESTED", "  用户执行请求",
                          cleonos_sys_user_exec_requested());
    ush_print_kv_hex_i18n("  USER_LAUNCH_TRIES", "  用户启动尝试",
                          cleonos_sys_user_launch_tries());
    ush_print_kv_hex_i18n("  USER_LAUNCH_OK", "  用户启动成功",
                          cleonos_sys_user_launch_ok());
    ush_print_kv_hex_i18n("  USER_LAUNCH_FAIL", "  用户启动失败",
                          cleonos_sys_user_launch_fail());
    ush_print_kv_hex_i18n("  EXEC_REQUESTS", "  执行请求", cleonos_sys_exec_request_count());
    ush_print_kv_hex_i18n("  EXEC_SUCCESS", "  执行成功", cleonos_sys_exec_success_count());
    ush_print_kv_hex_i18n("  TTY_COUNT", "  TTY 数量", cleonos_sys_tty_count());
    ush_print_kv_hex_i18n("  TTY_ACTIVE", "  当前 TTY", cleonos_sys_tty_active());
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "userstat") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_userstat();

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
