#include "cmd_runtime.h"

#include <stdio.h>

#include "browser/browser_internal.h"

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    char argv_buf[USH_BROWSER_SOURCE_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";
    u64 argc = 0ULL;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_zero(argv_buf, (u64)sizeof(argv_buf));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "browser") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
            }
        }
    }

    if (has_context == 0) {
        argc = cleonos_sys_proc_argc();
        if (argc > 1ULL) {
            (void)cleonos_sys_proc_argv(1ULL, argv_buf, (u64)sizeof(argv_buf));
            arg = argv_buf;
        }
    }

    success = ush_browser_run_session(&sh, arg);

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
