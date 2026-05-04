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
    u64 present;
    u64 size_bytes;
    u64 sectors;
    u64 formatted;
    u64 mounted;

    present = cleonos_sys_disk_present();
    if (present == 0ULL) {
        ush_writeln_i18n("mkfsfat32: disk not present", "mkfsfat32: 磁盘不存在");
        return 0;
    }

    if (ush_arg_parse_label(arg, label, (u64)sizeof(label)) == 0) {
        ush_writeln_i18n("mkfsfat32: usage mkfsfat32 [label]", "mkfsfat32: 用法 mkfsfat32 [label]");
        return 0;
    }

    ok = cleonos_sys_disk_format_fat32((label[0] != '\0') ? label : (const char *)0);
    if (ok == 0ULL) {
        char line[USH_LINE_MAX];

        size_bytes = cleonos_sys_disk_size_bytes();
        sectors = cleonos_sys_disk_sector_count();
        formatted = cleonos_sys_disk_formatted();
        mounted = cleonos_sys_disk_mounted();
        ush_writeln_i18n("mkfsfat32: format failed", "mkfsfat32: 格式化失败");
        (void)snprintf(line, (unsigned long)sizeof(line),
                       (ush_locale_is_zh() != 0)
                           ? "mkfsfat32: disk.存在=%llu size=%llu sectors=%llu formatted=%llu mounted=%llu"
                           : "mkfsfat32: disk.present=%llu size=%llu sectors=%llu formatted=%llu mounted=%llu",
                       (unsigned long long)present, (unsigned long long)size_bytes, (unsigned long long)sectors,
                       (unsigned long long)formatted, (unsigned long long)mounted);
        ush_writeln(line);
        return 0;
    }

    if (label[0] != '\0') {
        char line[USH_LINE_MAX];

        (void)snprintf(line, (unsigned long)sizeof(line),
                       (ush_locale_is_zh() != 0) ? "mkfsfat32: 已格式化 (label=%s)" : "mkfsfat32: formatted (label=%s)",
                       label);
        ush_writeln(line);
    } else {
        ush_writeln_i18n("mkfsfat32: formatted", "mkfsfat32: 已格式化");
    }

    ush_writeln_i18n("mkfsfat32: now run 'mount /temp/disk' (or another mount path)",
                     "mkfsfat32: 现在运行 'mount /temp/disk'（或其他挂载路径）");
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
