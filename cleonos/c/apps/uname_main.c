#include "cmd_runtime.h"

static void uname_write_u64(u64 value) {
    char rev[32];
    u64 len = 0ULL;

    if (value == 0ULL) {
        ush_write_char('0');
        return;
    }

    while (value != 0ULL && len < (u64)sizeof(rev)) {
        rev[len++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    while (len > 0ULL) {
        ush_write_char(rev[--len]);
    }
}

static void uname_print_kv_text_i18n(const char *key, const char *zh, const char *value) {
    ush_write_i18n_label(key, zh);
    ush_write(": ");
    ush_writeln(value);
}

static void uname_print_kv_u64_i18n(const char *key, const char *zh, u64 value) {
    ush_write_i18n_label(key, zh);
    ush_write(": ");
    uname_write_u64(value);
    ush_write_char('\n');
}

static int uname_show_sysinfo(void) {
    cleonos_sysinfo info;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_sysinfo(&info) == 0ULL) {
        ush_writeln_i18n("uname: sysinfo syscall failed", "uname: sysinfo 系统调用失败");
        return 0;
    }

    uname_print_kv_text_i18n("Kernel", "内核", info.kernel_name);
    uname_print_kv_text_i18n("Version", "版本", info.kernel_version);
    uname_print_kv_text_i18n("Arch", "架构", info.arch);
    uname_print_kv_text_i18n("BuildDate", "构建日期", info.build_date);
    uname_print_kv_text_i18n("BuildTime", "构建时间", info.build_time);
    uname_print_kv_text_i18n("BootMode", "启动模式", info.boot_mode);
    uname_print_kv_u64_i18n("UptimeMs", "运行时间毫秒", info.uptime_ms);
    uname_print_kv_u64_i18n("TimerTicks", "计时器滴答", info.timer_ticks);
    uname_print_kv_u64_i18n("TimerHz", "计时器频率", info.timer_hz);
    uname_print_kv_u64_i18n("ManagedPages", "管理页数", info.managed_pages);
    uname_print_kv_u64_i18n("FreePages", "空闲页数", info.free_pages);
    uname_print_kv_u64_i18n("UsedPages", "已用页数", info.used_pages);
    uname_print_kv_u64_i18n("DroppedPages", "丢弃页数", info.dropped_pages);
    uname_print_kv_u64_i18n("HeapTotalBytes", "堆总字节", info.heap_total_bytes);
    uname_print_kv_u64_i18n("HeapUsedBytes", "堆已用字节", info.heap_used_bytes);
    uname_print_kv_u64_i18n("HeapFreeBytes", "堆空闲字节", info.heap_free_bytes);
    uname_print_kv_u64_i18n("FSNodes", "文件系统节点", info.fs_nodes);
    uname_print_kv_u64_i18n("Tasks", "任务数", info.task_count);
    uname_print_kv_u64_i18n("Services", "服务数", info.service_count);
    uname_print_kv_u64_i18n("ServicesReady", "就绪服务数", info.service_ready_count);
    return 1;
}

static int uname_show_short(int all) {
    cleonos_sysinfo info;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_sysinfo(&info) == 0ULL) {
        ush_writeln_i18n("uname: sysinfo syscall failed", "uname: sysinfo 系统调用失败");
        return 0;
    }

    ush_write(info.kernel_name);
    if (all != 0) {
        ush_write(" ");
        ush_write(info.arch);
        ush_write(" ");
        ush_write(info.kernel_version);
        ush_write(" ");
        ush_write(info.boot_mode);
        ush_write(" ");
        ush_write(info.build_date);
        ush_write(" ");
        ush_write(info.build_time);
    }
    ush_write_char('\n');
    return 1;
}

static int ush_cmd_uname(const char *arg) {
    char first[32];
    const char *rest;

    if (arg == (const char *)0 || arg[0] == '\0') {
        return uname_show_short(0);
    }

    if (ush_split_first_and_rest(arg, first, (u64)sizeof(first), &rest) == 0 || rest[0] != '\0') {
        ush_writeln_i18n("usage: uname [-a|--all|--sysinfo]", "用法: uname [-a|--all|--sysinfo]");
        return 0;
    }

    if (ush_streq(first, "-a") != 0 || ush_streq(first, "--all") != 0) {
        return uname_show_short(1);
    }

    if (ush_streq(first, "--sysinfo") != 0 || ush_streq(first, "-s") != 0) {
        return uname_show_sysinfo();
    }

    ush_writeln_i18n("usage: uname [-a|--all|--sysinfo]", "用法: uname [-a|--all|--sysinfo]");
    return 0;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    const char *arg = "";
    int success;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cwd[0] == '/') {
            ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
        }
        arg = ctx.arg;
    }

    success = ush_cmd_uname(arg);
    ret.exit_code = (success != 0) ? 0ULL : 1ULL;
    (void)ush_command_ret_write(&ret);
    return success != 0 ? 0 : 1;
}
