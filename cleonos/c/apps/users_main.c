#include "user_cmd_common.h"

static int users_run(void) {
    cleonos_user_info current;
    u64 count;
    u64 i;
    int listed = 0;

    if (ucmd_query_current_user(&current) == 0 || ucmd_require_admin("users", &current) == 0) {
        return 0;
    }

    count = cleonos_sys_user_count();
    if (count == (u64)-1 || count == 0ULL) {
        ush_writeln("users: no users or users.db unreadable");
        return 0;
    }

    for (i = 0ULL; i < count; i++) {
        cleonos_user_info info;

        if (cleonos_sys_user_at(i, &info) == 0ULL) {
            continue;
        }

        ucmd_emit_user_info(&info);
        listed++;
    }

    if (listed == 0) {
        ush_writeln("users: no users or users.db unreadable");
        return 0;
    }

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
    ucmd_load_context(&ctx, &sh, "users", initial_cwd, &has_context, &arg);
    (void)arg;
    success = users_run();
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
