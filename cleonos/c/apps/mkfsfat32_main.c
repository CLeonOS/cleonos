#include "cmd_runtime.h"
#include <stdio.h>

static int ush_arg_parse_label(const char *arg, char *out_label, u64 out_label_size) {
    char first[USH_ARG_MAX];
    const char *rest = (const char *)0;

    if (out_label == (char *)0 || out_label_size == 0ULL) {
        return 0;
    }

    out_label[0] = '\0';

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0) {
        return 0;
    }

    while (rest != (const char *)0 && *rest != '\0' && ush_is_space(*rest) != 0) {
        rest++;
    }

    if (rest != (const char *)0 && *rest != '\0') {
        return 0;
    }

    ush_copy(out_label, out_label_size, first);
    return 1;
}

static int ush_cmd_mkfsfat32(const char *arg) {
    char label[16];
    u64 ok;

    if (cleonos_sys_disk_present() == 0ULL) {
        (void)fputs("mkfsfat32: disk not present\n", 1);
        return 0;
    }

    if (ush_arg_parse_label(arg, label, (u64)sizeof(label)) == 0) {
        (void)fputs("mkfsfat32: usage mkfsfat32 [label]\n", 1);
        return 0;
    }

    ok = cleonos_sys_disk_format_fat32((label[0] != '\0') ? label : (const char *)0);
    if (ok == 0ULL) {
        (void)fputs("mkfsfat32: format failed\n", 1);
        return 0;
    }

    if (label[0] != '\0') {
        (void)printf("mkfsfat32: formatted (label=%s)\n", label);
    } else {
        (void)fputs("mkfsfat32: formatted\n", 1);
    }

    (void)fputs("mkfsfat32: now run 'mount /temp/disk' (or another mount path)\n", 1);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "mkfsfat32") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_mkfsfat32(arg);

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
