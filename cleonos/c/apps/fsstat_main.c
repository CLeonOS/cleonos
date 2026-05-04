#include "cmd_runtime.h"
static int ush_cmd_fsstat(void) {
    ush_writeln_i18n("fsstat:", "文件系统统计 (fsstat):");
    ush_print_kv_hex_i18n("  NODE_COUNT", "  节点数 (NODE_COUNT)", cleonos_sys_fs_node_count());
    ush_print_kv_hex_i18n("  ROOT_CHILDREN", "  根目录子项 (ROOT_CHILDREN)", cleonos_sys_fs_child_count("/"));
    ush_print_kv_hex_i18n("  SYSTEM_CHILDREN", "  系统目录子项 (SYSTEM_CHILDREN)",
                          cleonos_sys_fs_child_count("/system"));
    ush_print_kv_hex_i18n("  SHELL_CHILDREN", "  外壳目录子项 (SHELL_CHILDREN)",
                          cleonos_sys_fs_child_count("/shell"));
    ush_print_kv_hex_i18n("  TEMP_CHILDREN", "  临时目录子项 (TEMP_CHILDREN)",
                          cleonos_sys_fs_child_count("/temp"));
    ush_print_kv_hex_i18n("  DRIVER_CHILDREN", "  驱动目录子项 (DRIVER_CHILDREN)",
                          cleonos_sys_fs_child_count("/driver"));
    ush_print_kv_hex_i18n("  DEV_CHILDREN", "  设备目录子项 (DEV_CHILDREN)", cleonos_sys_fs_child_count("/dev"));
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "fsstat") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_fsstat();

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
