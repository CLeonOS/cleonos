#include "cmd_runtime.h"
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "shstat") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_shstat(&sh);

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
