#include "cmd_runtime.h"
#include <cleonos_syscall.h>
#include <stdio.h>
#include <string.h>

static void fsck_print_result(const cleonos_disk_fsck_result *result) {
    (void)printf((ush_locale_is_zh() != 0) ? "fsckfat32: 状态: %s\n" : "fsckfat32: status: %s\n",
                 (result->status == 0ULL) ? "clean" : "issues-found");
    (void)printf("fsckfat32: checked_clusters: %llu\n", (unsigned long long)result->checked_clusters);
    (void)printf("fsckfat32: free_clusters: %llu\n", (unsigned long long)result->free_clusters);
    (void)printf("fsckfat32: used_clusters: %llu\n", (unsigned long long)result->used_clusters);
    (void)printf("fsckfat32: bad_entries: %llu\n", (unsigned long long)result->bad_entries);
    (void)printf("fsckfat32: loops: %llu\n", (unsigned long long)result->loops);
    (void)printf("fsckfat32: size_mismatches: %llu\n", (unsigned long long)result->size_mismatches);
    (void)printf("fsckfat32: orphan_clusters: %llu\n", (unsigned long long)result->orphan_clusters);
    (void)printf("fsckfat32: fixed_entries: %llu\n", (unsigned long long)result->fixed_entries);
    (void)printf("fsckfat32: fixed_orphans: %llu\n", (unsigned long long)result->fixed_orphans);
}

static int fsckfat32_run(int argc, char **argv) {
    cleonos_disk_fsck_result result;
    u64 flags = 0ULL;
    int i;

    for (i = 1; i < argc; i++) {
        const char *arg = (argv != (char **)0) ? argv[i] : (const char *)0;
        if (arg == (const char *)0) {
            continue;
        }
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            ush_writeln_i18n("usage: fsckfat32 [--fix]", "用法: fsckfat32 [--fix]");
            return 1;
        }
        if (strcmp(arg, "--fix") == 0 || strcmp(arg, "-f") == 0) {
            flags |= CLEONOS_DISK_FSCK_FLAG_FIX;
            continue;
        }
        (void)printf((ush_locale_is_zh() != 0) ? "fsckfat32: 未知选项: %s\n"
                                                : "fsckfat32: unknown option: %s\n",
                     arg);
        ush_writeln_i18n("usage: fsckfat32 [--fix]", "用法: fsckfat32 [--fix]");
        return 0;
    }

    if (cleonos_sys_disk_present() == 0ULL) {
        ush_writeln_i18n("fsckfat32: disk not present", "fsckfat32: 磁盘不存在");
        return 0;
    }

    if (cleonos_sys_disk_formatted() == 0ULL) {
        ush_writeln_i18n("fsckfat32: disk is not FAT32/formatted", "fsckfat32: 磁盘不是 FAT32 或尚未格式化");
        return 0;
    }

    if (cleonos_sys_disk_fsck_fat32(flags, &result) == 0ULL) {
        ush_writeln_i18n("fsckfat32: check failed", "fsckfat32: 检查失败");
        return 0;
    }

    fsck_print_result(&result);
    return (result.status == 0ULL || (flags & CLEONOS_DISK_FSCK_FLAG_FIX) != 0ULL) ? 1 : 0;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    (void)envp;
    return (fsckfat32_run(argc, argv) != 0) ? 0 : 1;
}
