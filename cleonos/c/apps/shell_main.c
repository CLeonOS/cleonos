#include "shell/shell_internal.h"

static int shell_locale_is_zh(void) {
    char locale[16];

    locale[0] = '\0';
    if (cleonos_sys_locale_get(locale, (u64)sizeof(locale)) == 0ULL) {
        return 0;
    }

    return (locale[0] == 'z' && locale[1] == 'h') ? 1 : 0;
}

static void shell_writeln_i18n(const char *en, const char *zh) {
    ush_writeln((shell_locale_is_zh() != 0) ? zh : en);
}

int cleonos_app_main(void) {
    ush_state sh;
    char line[USH_LINE_MAX];

    ush_init_state(&sh);
    shell_writeln_i18n("\x1B[92m[USER][SHELL]\x1B[0m interactive framework online",
                       "\x1B[92m[USER][SHELL]\x1B[0m 交互框架在线 (interactive framework online)");

    if (ush_login_if_needed(&sh) == 0) {
        return 1;
    }

    if (ush_run_script_file(&sh, "/shell/init.cmd") == 0 && ush_run_script_file(&sh, "/shell/INIT.CMD") == 0 &&
        ush_run_script_file(&sh, "/SHELL/INIT.CMD") == 0) {
        shell_writeln_i18n("\x1B[33m[USER][SHELL]\x1B[0m init script not found, continue interactive mode",
                           "\x1B[33m[USER][SHELL]\x1B[0m 未找到初始化脚本，继续交互模式 (init script not found)");
    }

    for (;;) {
        ush_read_line(&sh, line, (u64)sizeof(line));
        ush_execute_line(&sh, line);

        if (sh.exit_requested != 0) {
            return (int)(sh.exit_code & 0x7FFFFFFFULL);
        }
    }
}
