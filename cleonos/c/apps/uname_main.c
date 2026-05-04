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

static void uname_print_kv_text(const char *key, const char *value) {
    ush_write(key);
    ush_write(": ");
    ush_writeln(value);
}

static void uname_print_kv_u64(const char *key, u64 value) {
    ush_write(key);
    ush_write(": ");
    uname_write_u64(value);
    ush_write_char('\n');
}

static int uname_show_sysinfo(void) {
    cleonos_sysinfo info;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_sysinfo(&info) == 0ULL) {
        ush_writeln("uname: sysinfo syscall failed");
        return 0;
    }

    uname_print_kv_text("Kernel", info.kernel_name);
    uname_print_kv_text("Version", info.kernel_version);
    uname_print_kv_text("Arch", info.arch);
    uname_print_kv_text("BuildDate", info.build_date);
    uname_print_kv_text("BuildTime", info.build_time);
    uname_print_kv_text("BootMode", info.boot_mode);
    uname_print_kv_u64("UptimeMs", info.uptime_ms);
    uname_print_kv_u64("TimerTicks", info.timer_ticks);
    uname_print_kv_u64("TimerHz", info.timer_hz);
    uname_print_kv_u64("ManagedPages", info.managed_pages);
    uname_print_kv_u64("FreePages", info.free_pages);
    uname_print_kv_u64("UsedPages", info.used_pages);
    uname_print_kv_u64("DroppedPages", info.dropped_pages);
    uname_print_kv_u64("HeapTotalBytes", info.heap_total_bytes);
    uname_print_kv_u64("HeapUsedBytes", info.heap_used_bytes);
    uname_print_kv_u64("HeapFreeBytes", info.heap_free_bytes);
    uname_print_kv_u64("FSNodes", info.fs_nodes);
    uname_print_kv_u64("Tasks", info.task_count);
    uname_print_kv_u64("Services", info.service_count);
    uname_print_kv_u64("ServicesReady", info.service_ready_count);
    return 1;
}

static int uname_show_short(int all) {
    cleonos_sysinfo info;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_sysinfo(&info) == 0ULL) {
        ush_writeln("uname: sysinfo syscall failed");
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
        ush_writeln("usage: uname [-a|--all|--sysinfo]");
        return 0;
    }

    if (ush_streq(first, "-a") != 0 || ush_streq(first, "--all") != 0) {
        return uname_show_short(1);
    }

    if (ush_streq(first, "--sysinfo") != 0 || ush_streq(first, "-s") != 0) {
        return uname_show_sysinfo();
    }

    ush_writeln("usage: uname [-a|--all|--sysinfo]");
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
