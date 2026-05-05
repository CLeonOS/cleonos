#include "user_cmd_common.h"

static int whoami_run(void) {
    cleonos_user_info info;

    if (ucmd_query_current_user(&info) == 0 || info.logged_in == 0ULL || info.name[0] == '\0') {
        ush_writeln("user");
        return 1;
    }

    ush_writeln(info.name);
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
    ucmd_load_context(&ctx, &sh, "whoami", initial_cwd, &has_context, &arg);
    (void)arg;
    success = whoami_run();
    ucmd_finish_context(&sh, initial_cwd, has_context);
    return (success != 0) ? 0 : 1;
}
