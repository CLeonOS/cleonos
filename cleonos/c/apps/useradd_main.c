#include "user_cmd_common.h"

static int useradd_run(const char *arg) {
    cleonos_user_info info;
    char opt[CLEONOS_USER_NAME_MAX];
    char name[CLEONOS_USER_NAME_MAX];
    const char *rest = "";
    u64 role = CLEONOS_USER_ROLE_USER;
    char password[96];
    char confirm[96];

    if (ucmd_query_current_user(&info) == 0 || ucmd_require_admin("useradd", &info) == 0) {
        return 0;
    }

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln("useradd: usage useradd [-a|--admin] <name>");
        return 0;
    }

    if (ush_split_first_and_rest(arg, opt, (u64)sizeof(opt), &rest) == 0) {
        ush_writeln("useradd: usage useradd [-a|--admin] <name>");
        return 0;
    }

    if (ush_streq(opt, "-a") != 0 || ush_streq(opt, "--admin") != 0) {
        role = CLEONOS_USER_ROLE_ADMIN;
        if (ush_split_first_and_rest(rest, name, (u64)sizeof(name), &rest) == 0) {
            ush_writeln("useradd: username required");
            return 0;
        }
    } else {
        ush_copy(name, (u64)sizeof(name), opt);
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        ush_writeln("useradd: too many arguments");
        return 0;
    }

    if (cleonos_user_name_valid(name) == 0) {
        ush_writeln("useradd: invalid username");
        return 0;
    }

    if (ucmd_read_secret_line("new user password: ", password, (u64)sizeof(password)) == 0 ||
        ucmd_read_secret_line("confirm password: ", confirm, (u64)sizeof(confirm)) == 0) {
        ush_writeln("useradd: password input failed");
        return 0;
    }

    if (password[0] == '\0') {
        ush_writeln("useradd: empty password rejected");
        return 0;
    }

    if (ush_streq(password, confirm) == 0) {
        ush_writeln("useradd: passwords do not match");
        return 0;
    }

    if (cleonos_user_create(name, password, role, 0) == 0) {
        ush_writeln("useradd: failed");
        return 0;
    }

    ush_writeln("useradd: user created");
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
    ucmd_load_context(&ctx, &sh, "useradd", initial_cwd, &has_context, &arg);
    success = useradd_run(arg);
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
