#include "cmd_runtime.h"
#include <stdio.h>

static int ush_mount_parse_path_arg(const char *arg, char *out_path, u64 out_path_size) {
    char first[USH_ARG_MAX];
    const char *rest = (const char *)0;

    if (out_path == (char *)0 || out_path_size == 0ULL) {
        return 0;
    }

    out_path[0] = '\0';

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

    ush_copy(out_path, out_path_size, first);
    return 1;
}

static int ush_cmd_mount(ush_state *sh, const char *arg) {
    char mount_arg[USH_PATH_MAX];
    char mount_path[USH_PATH_MAX];
    char current_path[USH_PATH_MAX];
    u64 mounted;

    if (sh == (ush_state *)0) {
        return 0;
    }

    if (cleonos_sys_disk_present() == 0ULL) {
        (void)fputs("mount: disk not present\n", 1);
        return 0;
    }

    if (ush_mount_parse_path_arg(arg, mount_arg, (u64)sizeof(mount_arg)) == 0) {
        (void)fputs("mount: usage mount [path]\n", 1);
        return 0;
    }

    mounted = cleonos_sys_disk_mounted();
    if (mount_arg[0] == '\0') {
        if (mounted == 0ULL) {
            (void)fputs("mount: no mounted disk path\n", 1);
            (void)fputs("mount: usage mount /temp/disk\n", 1);
            return 0;
        }

        current_path[0] = '\0';
        (void)cleonos_sys_disk_mount_path(current_path, (u64)sizeof(current_path));
        if (current_path[0] == '\0') {
            (void)fputs("disk0 on (unknown) type fat32\n", 1);
        } else {
            (void)printf("disk0 on %s type fat32\n", current_path);
        }
        return 1;
    }

    if (cleonos_sys_disk_formatted() == 0ULL) {
        (void)fputs("mount: disk is not FAT32 formatted\n", 1);
        (void)fputs("mount: run mkfsfat32 first\n", 1);
        return 0;
    }

    if (ush_resolve_path(sh, mount_arg, mount_path, (u64)sizeof(mount_path)) == 0) {
        (void)fputs("mount: invalid path\n", 1);
        return 0;
    }

    if (cleonos_sys_disk_mount(mount_path) == 0ULL) {
        (void)fputs("mount: mount failed\n", 1);
        return 0;
    }

    (void)printf("disk0 mounted on %s\n", mount_path);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "mount") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_mount(&sh, arg);

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
