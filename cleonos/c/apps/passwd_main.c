#include "user_cmd_common.h"

static int passwd_run(const char *arg) {
    cleonos_user_info info;
    char target[CLEONOS_USER_NAME_MAX];
    char old_password[96];
    char new_password[96];
    char confirm[96];

    if (ucmd_query_current_user(&info) == 0 || ucmd_require_disk_accounts("passwd", &info) == 0) {
        return 0;
    }

    target[0] = '\0';
    if (arg != (const char *)0 && arg[0] != '\0') {
        if (info.role != CLEONOS_USER_ROLE_ADMIN) {
            ush_writeln("passwd: only admin can change another user's password");
            return 0;
        }

        ush_copy(target, (u64)sizeof(target), arg);
        ush_trim_line(target);
    } else {
        ush_copy(target, (u64)sizeof(target), info.name);
    }

    if (cleonos_user_name_valid(target) == 0) {
        ush_writeln("passwd: invalid username");
        return 0;
    }

    old_password[0] = '\0';
    if (info.role != CLEONOS_USER_ROLE_ADMIN || ush_streq(target, info.name) != 0) {
        if (ucmd_read_secret_line("current password: ", old_password, (u64)sizeof(old_password)) == 0) {
            ush_writeln("passwd: current password required");
            return 0;
        }
    }

    if (ucmd_read_secret_line("new password: ", new_password, (u64)sizeof(new_password)) == 0 ||
        ucmd_read_secret_line("confirm new password: ", confirm, (u64)sizeof(confirm)) == 0) {
        ush_writeln("passwd: password input failed");
        return 0;
    }

    if (new_password[0] == '\0') {
        ush_writeln("passwd: empty password rejected");
        return 0;
    }

    if (ush_streq(new_password, confirm) == 0) {
        ush_writeln("passwd: passwords do not match");
        return 0;
    }

    if (cleonos_sys_user_passwd(target, old_password, new_password) == 0ULL) {
        ush_writeln("passwd: failed");
        return 0;
    }

    ush_writeln("passwd: password updated");
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
    ucmd_load_context(&ctx, &sh, "passwd", initial_cwd, &has_context, &arg);
    success = passwd_run(arg);
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
