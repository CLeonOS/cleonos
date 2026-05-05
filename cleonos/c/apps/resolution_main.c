#include "cmd_runtime.h"
#include <stdio.h>

static void res_print_mode(const char *label, u64 target) {
    cleonos_display_info info;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_display_info(target, &info) == 0ULL) {
        (void)printf("%s: unavailable\n", label);
        return;
    }

    (void)printf("%s: logical=%llux%llu physical=%llux%llu\n", label,
                 (unsigned long long)info.logical_width, (unsigned long long)info.logical_height,
                 (unsigned long long)info.physical_width, (unsigned long long)info.physical_height);
}

static int res_set_target(u64 target, const char *width_text, const char *height_text) {
    cleonos_display_set_mode_req req;
    u64 width;
    u64 height;

    if (ush_parse_u64_dec(width_text, &width) == 0 || ush_parse_u64_dec(height_text, &height) == 0) {
        return 0;
    }

    req.target = target;
    req.logical_width = width;
    req.logical_height = height;
    return (cleonos_sys_display_set_mode(&req) != 0ULL) ? 1 : 0;
}

static void res_usage(void) {
    ush_writeln_i18n("usage: resolution [show|tty <w> <h>|wm <w> <h>|all <w> <h>]",
                     "用法: resolution [show|tty <w> <h>|wm <w> <h>|all <w> <h>]");
}

static int ush_cmd_resolution(const char *arg) {
    char first[32];
    char second[32];
    char third[32];
    const char *rest = "";
    const char *rest2 = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        res_print_mode("tty", CLEONOS_DISPLAY_TARGET_TTY);
        res_print_mode("wm", CLEONOS_DISPLAY_TARGET_WM);
        return 1;
    }

    if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
        res_usage();
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        res_usage();
        return 0;
    }
    if (ush_split_first_and_rest(rest, second, (u64)sizeof(second), &rest2) == 0) {
        res_usage();
        return 0;
    }
    if (ush_split_first_and_rest(rest2, third, (u64)sizeof(third), &rest) == 0) {
        res_usage();
        return 0;
    }
    if (rest != (const char *)0 && rest[0] != '\0') {
        res_usage();
        return 0;
    }

    if (ush_streq(first, "tty") != 0) {
        if (res_set_target(CLEONOS_DISPLAY_TARGET_TTY, second, third) == 0) {
            ush_writeln_i18n("resolution: tty set failed", "resolution: TTY 设置失败");
            return 0;
        }
    } else if (ush_streq(first, "wm") != 0) {
        if (res_set_target(CLEONOS_DISPLAY_TARGET_WM, second, third) == 0) {
            ush_writeln_i18n("resolution: wm set failed", "resolution: WM 设置失败");
            return 0;
        }
    } else if (ush_streq(first, "all") != 0) {
        if (res_set_target(CLEONOS_DISPLAY_TARGET_TTY, second, third) == 0 ||
            res_set_target(CLEONOS_DISPLAY_TARGET_WM, second, third) == 0) {
            ush_writeln_i18n("resolution: set failed", "resolution: 设置失败");
            return 0;
        }
    } else {
        res_usage();
        return 0;
    }

    res_print_mode("tty", CLEONOS_DISPLAY_TARGET_TTY);
    res_print_mode("wm", CLEONOS_DISPLAY_TARGET_WM);
    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;
    const char *arg = "";

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "resolution") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_resolution(arg);

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
