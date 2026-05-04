#include "cmd_runtime.h"
static int ush_cmd_kbdstat(void) {
    ush_writeln_i18n("kbdstat:", "键盘统计 (kbdstat):");
    ush_print_kv_hex_i18n("  BUFFERED", "  缓冲数量", cleonos_sys_kbd_buffered());
    ush_print_kv_hex_i18n("  PUSHED", "  推入次数", cleonos_sys_kbd_pushed());
    ush_print_kv_hex_i18n("  POPPED", "  弹出次数", cleonos_sys_kbd_popped());
    ush_print_kv_hex_i18n("  DROPPED", "  丢弃次数", cleonos_sys_kbd_dropped());
    ush_print_kv_hex_i18n("  HOTKEY_SWITCHES", "  热键切换次数", cleonos_sys_kbd_hotkey_switches());
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "kbdstat") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_kbdstat();

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
