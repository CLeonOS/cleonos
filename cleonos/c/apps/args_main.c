#include "cmd_runtime.h"

static void ush_args_write_u64_dec(u64 value) {
    char tmp[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        ush_write_char('0');
        return;
    }

    while (value > 0ULL && len < (u64)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        len--;
        ush_write_char(tmp[len]);
    }
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    int i;
    int env_count = 0;

    ush_write_i18n_label("argc", "参数数量");
    ush_write("=");
    ush_args_write_u64_dec((u64)((argc >= 0) ? argc : 0));
    ush_write_char('\n');

    for (i = 0; i < argc; i++) {
        ush_write_i18n_label("argv", "参数");
        ush_write("[");
        ush_args_write_u64_dec((u64)i);
        ush_write("]=");

        if (argv != (char **)0 && argv[i] != (char *)0) {
            ush_writeln(argv[i]);
        } else {
            ush_writeln("(null)");
        }
    }

    if (envp != (char **)0) {
        while (envp[env_count] != (char *)0 && env_count < 64) {
            env_count++;
        }
    }

    ush_write_i18n_label("envc", "环境变量数量");
    ush_write("=");
    ush_args_write_u64_dec((u64)env_count);
    ush_write_char('\n');

    for (i = 0; i < env_count; i++) {
        ush_write_i18n_label("env", "环境变量");
        ush_write("[");
        ush_args_write_u64_dec((u64)i);
        ush_write("]=");
        ush_writeln(envp[i]);
    }

    return 0;
}
