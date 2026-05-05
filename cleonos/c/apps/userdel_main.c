#include "user_cmd_common.h"

static int userdel_run(const char *arg) {
    cleonos_user_info info;
    char name[CLEONOS_USER_NAME_MAX];

    if (ucmd_query_current_user(&info) == 0 || ucmd_require_admin("userdel", &info) == 0) {
        return 0;
    }

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("userdel: usage userdel <name>");
        return 0;
    }

    ush_copy(name, (u64)sizeof(name), arg);
    ush_trim_line(name);

    if (cleonos_user_remove(name) == 0) {
        ush_writeln("userdel: failed");
        return 0;
    }

    ush_writeln("userdel: user removed");
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
    ucmd_load_context(&ctx, &sh, "userdel", initial_cwd, &has_context, &arg);
    success = userdel_run(arg);
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
