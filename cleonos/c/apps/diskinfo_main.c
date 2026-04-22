#include "cmd_runtime.h"
#include <stdio.h>

static void clio_writeln(const char *text) {
    (void)fputs(text, 1);
    (void)putchar('\n');
}

static int ush_cmd_diskinfo(void) {
    u64 present = cleonos_sys_disk_present();
    u64 size_bytes = 0ULL;
    u64 sectors = 0ULL;
    u64 formatted = 0ULL;
    u64 mounted = 0ULL;
    char mount_path[USH_PATH_MAX];

    if (present == 0ULL) {
        clio_writeln("disk: not present");
        return 0;
    }

    size_bytes = cleonos_sys_disk_size_bytes();
    sectors = cleonos_sys_disk_sector_count();
    formatted = cleonos_sys_disk_formatted();
    mounted = cleonos_sys_disk_mounted();
    mount_path[0] = '\0';

    if (mounted != 0ULL) {
        (void)cleonos_sys_disk_mount_path(mount_path, (u64)sizeof(mount_path));
    }

    (void)printf("disk.present: %llu\n", (unsigned long long)present);
    (void)printf("disk.size_bytes: %llu\n", (unsigned long long)size_bytes);
    (void)printf("disk.sectors: %llu\n", (unsigned long long)sectors);
    (void)printf("disk.formatted_fat32: %llu\n", (unsigned long long)formatted);
    (void)printf("disk.mounted: %llu\n", (unsigned long long)mounted);

    if (mounted != 0ULL && mount_path[0] != '\0') {
        (void)printf("disk.mount_path: %s\n", mount_path);
    } else {
        clio_writeln("disk.mount_path: (none)");
    }

    return 1;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success = 0;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "diskinfo") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_diskinfo();

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
