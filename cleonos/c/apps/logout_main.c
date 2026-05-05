#include "user_cmd_common.h"

static int logout_run(void) {
    cleonos_user_info info;

    if (ucmd_query_current_user(&info) == 0) {
        ush_writeln("logout: failed");
        return 0;
    }

    if (info.disk_login_required == 0ULL) {
        ush_writeln("logout: login is disabled in ISO temporary mode");
        return 0;
    }

    (void)cleonos_sys_user_logout();
    (void)cleonos_sys_fs_remove(CLEONOS_USER_SESSION_PATH);
    ush_writeln("logout: logged out");
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    const char *arg = "";
    int has_context = 0;
    int success;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_init_state(&sh);
    ucmd_load_context(&ctx, &sh, "logout", initial_cwd, &has_context, &arg);
    (void)arg;
    success = logout_run();
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
