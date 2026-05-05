#include "user_cmd_common.h"

static int usermod_run(const char *arg) {
    cleonos_user_info info;
    char mode[32];
    char name[CLEONOS_USER_NAME_MAX];
    const char *rest = "";
    u64 role;

    if (ucmd_query_current_user(&info) == 0 || ucmd_require_admin("usermod", &info) == 0) {
        return 0;
    }

    if (ush_split_first_and_rest(arg, mode, (u64)sizeof(mode), &rest) == 0 ||
        ush_split_first_and_rest(rest, name, (u64)sizeof(name), &rest) == 0 ||
        (rest != (const char *)0 && rest[0] != '\0')) {
        ush_writeln("usermod: usage usermod <admin|user> <name>");
        return 0;
    }

    if (ush_streq(mode, "admin") != 0) {
        role = CLEONOS_USER_ROLE_ADMIN;
    } else if (ush_streq(mode, "user") != 0) {
        role = CLEONOS_USER_ROLE_USER;
    } else {
        ush_writeln("usermod: role must be admin or user");
        return 0;
    }

    if (cleonos_user_set_role(name, role) == 0) {
        ush_writeln("usermod: failed");
        return 0;
    }

    ush_writeln("usermod: role updated");
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
    ucmd_load_context(&ctx, &sh, "usermod", initial_cwd, &has_context, &arg);
    success = usermod_run(arg);
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
