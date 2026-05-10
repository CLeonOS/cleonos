#include "cmd_runtime.h"

#include <stdio.h>

#define CONTROL_THEME_PATH "/system/theme.conf"
#define CONTROL_STARTUP_PATH "/system/startup.conf"
#define CONTROL_FONT_PATH "/system/font.conf"
#define CONTROL_NET_PATH "/system/net.conf"

static void control_usage(void) {
    ush_writeln("CLeonOS Control Center CLI");
    ush_writeln("usage: control <command> [args]");
    ush_writeln("");
    ush_writeln("commands:");
    ush_writeln("  status                         show full system settings summary");
    ush_writeln("  display show                   show tty/wm resolution and tty grid");
    ush_writeln("  display tty <w> <h>            set tty logical resolution");
    ush_writeln("  display wm <w> <h>             set window-manager logical resolution");
    ush_writeln("  display all <w> <h>            set both tty and wm resolution");
    ush_writeln("  font show                      show configured font files");
    ush_writeln("  font set <tty.ttf> [emoji.ttf] write /system/font.conf");
    ush_writeln("  locale show                    show locale");
    ush_writeln("  locale set <locale>            set kernel locale and persist it");
    ush_writeln("  users show                     show current user and user count");
    ush_writeln("  users list                     run users.elf");
    ush_writeln("  users add [--admin] <name>     run useradd.elf");
    ush_writeln("  users passwd [name]            run passwd.elf");
    ush_writeln("  users role <admin|user> <name> run usermod.elf");
    ush_writeln("  users remove <name>            run userdel.elf");
    ush_writeln("  net show                       show network parameters");
    ush_writeln("  net config dhcp|static ...     write /system/net.conf placeholder");
    ush_writeln("  inputm show                    list input methods");
    ush_writeln("  inputm use <index>             select input method");
    ush_writeln("  theme show                     show /system/theme.conf");
    ush_writeln("  theme set <name>               write /system/theme.conf");
    ush_writeln("  startup show                   show /system/startup.conf");
    ush_writeln("  startup add <command>          append startup command");
    ush_writeln("  startup clear                  clear startup commands");
    ush_writeln("  boot                           show boot args and boot mode");
}

static int control_exec(const char *path, const char *argv_line) {
    u64 rc;

    rc = cleonos_sys_exec_pathv(path, argv_line != (const char *)0 ? argv_line : "", "");
    if ((rc & 0x8000000000000000ULL) != 0ULL || rc != 0ULL) {
        (void)printf("control: command failed: %s ret=0x%llx\n", path, (unsigned long long)rc);
        return 0;
    }
    return 1;
}

static void control_print_ipv4(u64 ipv4_be) {
    (void)printf("%u.%u.%u.%u", (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL),
                 (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL),
                 (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL), (unsigned int)(ipv4_be & 0xFFULL));
}

static int control_read_text(const char *path, char *out, u64 out_size) {
    u64 size;
    u64 got;

    if (path == (const char *)0 || out == (char *)0 || out_size == 0ULL) {
        return 0;
    }
    out[0] = '\0';
    size = cleonos_sys_fs_stat_size(path);
    if (size == (u64)-1 || size == 0ULL) {
        return 0;
    }
    if (size >= out_size) {
        size = out_size - 1ULL;
    }
    got = cleonos_sys_fs_read(path, out, size);
    out[got] = '\0';
    return got != 0ULL ? 1 : 0;
}

static int control_write_text(const char *path, const char *text) {
    u64 len;

    if (path == (const char *)0 || text == (const char *)0) {
        return 0;
    }
    len = ush_strlen(text);
    return (cleonos_sys_fs_write(path, text, len) == len) ? 1 : 0;
}

static int control_append_line(const char *path, const char *line) {
    char buf[USH_ARG_MAX + 4U];
    u64 len;

    if (line == (const char *)0 || line[0] == '\0') {
        return 0;
    }
    ush_copy(buf, (u64)sizeof(buf), line);
    len = ush_strlen(buf);
    if (len + 1ULL >= (u64)sizeof(buf)) {
        return 0;
    }
    buf[len++] = '\n';
    buf[len] = '\0';
    return (cleonos_sys_fs_append(path, buf, len) == len) ? 1 : 0;
}

static void control_print_display_one(const char *label, u64 target) {
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

static int control_display_show(void) {
    cleonos_tty_grid_info grid;

    control_print_display_one("tty", CLEONOS_DISPLAY_TARGET_TTY);
    control_print_display_one("wm", CLEONOS_DISPLAY_TARGET_WM);
    ush_zero(&grid, (u64)sizeof(grid));
    if (cleonos_sys_tty_grid_info(&grid) != 0ULL) {
        (void)printf("tty grid: cols=%llu rows=%llu\n", (unsigned long long)grid.cols, (unsigned long long)grid.rows);
    }
    return 1;
}

static int control_display_set(u64 target, const char *w_text, const char *h_text) {
    cleonos_display_set_mode_req req;
    u64 w;
    u64 h;

    if (ush_parse_u64_dec(w_text, &w) == 0 || ush_parse_u64_dec(h_text, &h) == 0 || w == 0ULL || h == 0ULL) {
        ush_writeln("control: invalid resolution");
        return 0;
    }
    req.target = target;
    req.logical_width = w;
    req.logical_height = h;
    if (cleonos_sys_display_set_mode(&req) == 0ULL) {
        ush_writeln("control: display set failed");
        return 0;
    }
    return 1;
}

static int control_display(const char *arg) {
    char mode[32];
    char w[32];
    char h[32];
    const char *rest = "";
    const char *rest2 = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_display_show();
    }
    if (ush_split_first_and_rest(arg, mode, (u64)sizeof(mode), &rest) == 0 ||
        ush_split_first_and_rest(rest, w, (u64)sizeof(w), &rest2) == 0 ||
        ush_split_first_and_rest(rest2, h, (u64)sizeof(h), &rest) == 0 || rest[0] != '\0') {
        ush_writeln("usage: control display <tty|wm|all> <w> <h>");
        return 0;
    }
    if (ush_streq(mode, "tty") != 0) {
        return control_display_set(CLEONOS_DISPLAY_TARGET_TTY, w, h) != 0 && control_display_show() != 0;
    }
    if (ush_streq(mode, "wm") != 0) {
        return control_display_set(CLEONOS_DISPLAY_TARGET_WM, w, h) != 0 && control_display_show() != 0;
    }
    if (ush_streq(mode, "all") != 0) {
        return control_display_set(CLEONOS_DISPLAY_TARGET_TTY, w, h) != 0 &&
               control_display_set(CLEONOS_DISPLAY_TARGET_WM, w, h) != 0 && control_display_show() != 0;
    }
    ush_writeln("usage: control display <tty|wm|all> <w> <h>");
    return 0;
}

static int control_locale_show(void) {
    char locale[CLEONOS_LOCALE_TEXT_MAX];

    ush_zero(locale, (u64)sizeof(locale));
    if (cleonos_sys_locale_get(locale, (u64)sizeof(locale)) == 0ULL) {
        ush_writeln("locale: unavailable");
        return 0;
    }
    (void)printf("locale: %s\n", locale[0] != '\0' ? locale : "unknown");
    return 1;
}

static int control_locale(const char *arg) {
    char cmd[32];
    char value[CLEONOS_LOCALE_TEXT_MAX];
    const char *rest = "";
    u64 rc;

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0 || ush_streq(arg, "get") != 0) {
        return control_locale_show();
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0 || ush_streq(cmd, "set") == 0) {
        ush_writeln("usage: control locale set <locale>");
        return 0;
    }
    if (ush_split_first_and_rest(rest, value, (u64)sizeof(value), &rest) == 0 || rest[0] != '\0') {
        ush_writeln("usage: control locale set <locale>");
        return 0;
    }
    rc = cleonos_sys_locale_set(value);
    if (rc == 0ULL) {
        ush_writeln("control: locale set failed");
        return 0;
    }
    if (rc == 2ULL) {
        ush_writeln("control: locale changed in kernel, persist failed");
    }
    return control_locale_show();
}

static int control_net_show(void) {
    if (cleonos_sys_net_available() == 0ULL) {
        ush_writeln("network: unavailable");
        return 0;
    }
    ush_write("ipv4: ");
    control_print_ipv4(cleonos_sys_net_ipv4_addr());
    ush_write("\nnetmask: ");
    control_print_ipv4(cleonos_sys_net_netmask());
    ush_write("\ngateway: ");
    control_print_ipv4(cleonos_sys_net_gateway());
    ush_write("\ndns: ");
    control_print_ipv4(cleonos_sys_net_dns_server());
    ush_write("\n");
    return 1;
}

static int control_net(const char *arg) {
    char cmd[32];
    const char *rest = "";
    char buf[USH_ARG_MAX + 32U];

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_net_show();
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0 || ush_streq(cmd, "config") == 0 ||
        rest[0] == '\0') {
        ush_writeln("usage: control net config dhcp|static <ip> <mask> <gw> <dns>");
        return 0;
    }
    ush_copy(buf, (u64)sizeof(buf), "mode=");
    strncat(buf, rest, sizeof(buf) - strlen(buf) - 2U);
    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1U);
    if (control_write_text(CONTROL_NET_PATH, buf) == 0) {
        ush_writeln("control: failed to write /system/net.conf");
        return 0;
    }
    ush_writeln("control: wrote /system/net.conf (kernel live reconfigure syscall not available yet)");
    return 1;
}

static int control_inputm_show(void) {
    u64 count = cleonos_sys_inputm_count();
    u64 current = cleonos_sys_inputm_current();
    u64 i;

    (void)printf("input methods: %llu current=%llu\n", (unsigned long long)count, (unsigned long long)current);
    for (i = 0ULL; i < count; i++) {
        cleonos_inputm_info info;
        ush_zero(&info, (u64)sizeof(info));
        if (cleonos_sys_inputm_info(i, &info) != 0ULL) {
            (void)printf("%c %llu: %s label=%s path=%s rule=%s flags=0x%llx\n", (i == current) ? '*' : ' ',
                         (unsigned long long)i, info.name, info.label, info.path, info.rule_path,
                         (unsigned long long)info.flags);
        }
    }
    return 1;
}

static int control_inputm(const char *arg) {
    char cmd[32];
    char idx_text[32];
    const char *rest = "";
    u64 idx;

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0 || ush_streq(arg, "list") != 0) {
        return control_inputm_show();
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0 || ush_streq(cmd, "use") == 0 ||
        ush_split_first_and_rest(rest, idx_text, (u64)sizeof(idx_text), &rest) == 0 || rest[0] != '\0' ||
        ush_parse_u64_dec(idx_text, &idx) == 0) {
        ush_writeln("usage: control inputm use <index>");
        return 0;
    }
    if (cleonos_sys_inputm_select(idx) == 0ULL) {
        ush_writeln("control: input method select failed");
        return 0;
    }
    return control_inputm_show();
}

static int control_users_show(void) {
    cleonos_user_info info;
    u64 count;

    ush_zero(&info, (u64)sizeof(info));
    if (cleonos_sys_user_current(&info) == 0ULL) {
        ush_writeln("users: unavailable");
        return 0;
    }
    count = cleonos_sys_user_count();
    (void)printf("current user: %s uid=%llu role=%s home=%s logged_in=%llu disk_login_required=%llu\n",
                 info.name[0] != '\0' ? info.name : "(none)", (unsigned long long)info.uid,
                 info.role == CLEONOS_USER_ROLE_ADMIN ? "admin" : "user", info.home,
                 (unsigned long long)info.logged_in, (unsigned long long)info.disk_login_required);
    if (count != (u64)-1) {
        (void)printf("user count: %llu\n", (unsigned long long)count);
    }
    return 1;
}

static int control_users(const char *arg) {
    char cmd[32];
    const char *rest = "";
    char argv[USH_ARG_MAX];

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_users_show();
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        return control_users_show();
    }
    if (ush_streq(cmd, "list") != 0) {
        return control_exec("/shell/users.elf", "");
    }
    if (ush_streq(cmd, "add") != 0) {
        return control_exec("/shell/useradd.elf", rest);
    }
    if (ush_streq(cmd, "passwd") != 0) {
        return control_exec("/shell/passwd.elf", rest);
    }
    if (ush_streq(cmd, "role") != 0) {
        return control_exec("/shell/usermod.elf", rest);
    }
    if (ush_streq(cmd, "remove") != 0 || ush_streq(cmd, "del") != 0) {
        return control_exec("/shell/userdel.elf", rest);
    }
    ush_copy(argv, (u64)sizeof(argv), arg);
    (void)argv;
    ush_writeln("usage: control users [show|list|add|passwd|role|remove]");
    return 0;
}

static int control_file_show(const char *label, const char *path) {
    char data[512];

    ush_write(label);
    ush_write(": ");
    ush_writeln(path);
    if (control_read_text(path, data, (u64)sizeof(data)) == 0) {
        ush_writeln("(not configured)");
        return 1;
    }
    ush_writeln(data);
    return 1;
}

static int control_theme(const char *arg) {
    char cmd[32];
    char data[96];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_file_show("theme", CONTROL_THEME_PATH);
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0 || ush_streq(cmd, "set") == 0 ||
        rest[0] == '\0') {
        ush_writeln("usage: control theme set <name>");
        return 0;
    }
    ush_copy(data, (u64)sizeof(data), "theme=");
    strncat(data, rest, sizeof(data) - strlen(data) - 2U);
    strncat(data, "\n", sizeof(data) - strlen(data) - 1U);
    if (control_write_text(CONTROL_THEME_PATH, data) == 0) {
        ush_writeln("control: theme write failed");
        return 0;
    }
    return control_file_show("theme", CONTROL_THEME_PATH);
}

static int control_font(const char *arg) {
    char cmd[32];
    char tty_path[96];
    char emoji_path[96];
    char data[256];
    const char *rest = "";
    const char *rest2 = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_file_show("font", CONTROL_FONT_PATH);
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0 || ush_streq(cmd, "set") == 0 ||
        ush_split_first_and_rest(rest, tty_path, (u64)sizeof(tty_path), &rest2) == 0) {
        ush_writeln("usage: control font set <tty.ttf> [emoji.ttf]");
        return 0;
    }
    emoji_path[0] = '\0';
    if (rest2[0] != '\0') {
        if (ush_split_first_and_rest(rest2, emoji_path, (u64)sizeof(emoji_path), &rest) == 0 || rest[0] != '\0') {
            ush_writeln("usage: control font set <tty.ttf> [emoji.ttf]");
            return 0;
        }
    }
    snprintf(data, sizeof(data), "tty=%s\nemoji=%s\n", tty_path, emoji_path[0] != '\0' ? emoji_path : "/system/emoji.ttf");
    if (control_write_text(CONTROL_FONT_PATH, data) == 0) {
        ush_writeln("control: font write failed");
        return 0;
    }
    ush_writeln("control: font config written (reboot or font reload syscall required to apply)");
    return control_file_show("font", CONTROL_FONT_PATH);
}

static int control_startup(const char *arg) {
    char cmd[32];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "show") != 0) {
        return control_file_show("startup", CONTROL_STARTUP_PATH);
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        ush_writeln("usage: control startup [show|add <command>|clear]");
        return 0;
    }
    if (ush_streq(cmd, "clear") != 0) {
        if (control_write_text(CONTROL_STARTUP_PATH, "") == 0) {
            ush_writeln("control: startup clear failed");
            return 0;
        }
        ush_writeln("control: startup cleared");
        return 1;
    }
    if (ush_streq(cmd, "add") != 0 && rest[0] != '\0') {
        if (control_append_line(CONTROL_STARTUP_PATH, rest) == 0) {
            ush_writeln("control: startup append failed");
            return 0;
        }
        return control_file_show("startup", CONTROL_STARTUP_PATH);
    }
    ush_writeln("usage: control startup [show|add <command>|clear]");
    return 0;
}

static int control_boot(void) {
    cleonos_sysinfo info;
    char cmdline[512];

    ush_zero(&info, (u64)sizeof(info));
    ush_zero(cmdline, (u64)sizeof(cmdline));
    (void)cleonos_sys_sysinfo(&info);
    (void)cleonos_sys_boot_cmdline(cmdline, (u64)sizeof(cmdline));
    (void)printf("kernel: %s %s %s\n", info.kernel_name, info.kernel_version, info.arch);
    (void)printf("build: %s %s\n", info.build_date, info.build_time);
    (void)printf("boot mode: %s\n", info.boot_mode[0] != '\0' ? info.boot_mode : "unknown");
    (void)printf("cmdline: %s\n", cmdline[0] != '\0' ? cmdline : "(empty)");
    return 1;
}

static int control_status(void) {
    (void)control_boot();
    ush_writeln("--- display ---");
    (void)control_display_show();
    ush_writeln("--- locale ---");
    (void)control_locale_show();
    ush_writeln("--- user ---");
    (void)control_users_show();
    ush_writeln("--- network ---");
    (void)control_net_show();
    ush_writeln("--- input method ---");
    (void)control_inputm_show();
    ush_writeln("--- theme ---");
    (void)control_file_show("theme", CONTROL_THEME_PATH);
    ush_writeln("--- startup ---");
    (void)control_file_show("startup", CONTROL_STARTUP_PATH);
    return 1;
}

static int control_run(const char *arg) {
    char cmd[32];
    const char *rest = "";

    if (arg == (const char *)0 || arg[0] == '\0' || ush_streq(arg, "help") != 0 || ush_streq(arg, "--help") != 0 ||
        ush_streq(arg, "-h") != 0) {
        control_usage();
        return 1;
    }
    if (ush_split_first_and_rest(arg, cmd, (u64)sizeof(cmd), &rest) == 0) {
        control_usage();
        return 0;
    }
    if (ush_streq(cmd, "status") != 0) {
        return control_status();
    }
    if (ush_streq(cmd, "display") != 0 || ush_streq(cmd, "resolution") != 0) {
        return control_display(rest);
    }
    if (ush_streq(cmd, "font") != 0) {
        return control_font(rest);
    }
    if (ush_streq(cmd, "locale") != 0) {
        return control_locale(rest);
    }
    if (ush_streq(cmd, "users") != 0 || ush_streq(cmd, "user") != 0) {
        return control_users(rest);
    }
    if (ush_streq(cmd, "net") != 0 || ush_streq(cmd, "network") != 0) {
        return control_net(rest);
    }
    if (ush_streq(cmd, "inputm") != 0 || ush_streq(cmd, "ime") != 0) {
        return control_inputm(rest);
    }
    if (ush_streq(cmd, "theme") != 0) {
        return control_theme(rest);
    }
    if (ush_streq(cmd, "startup") != 0 || ush_streq(cmd, "bootapp") != 0) {
        return control_startup(rest);
    }
    if (ush_streq(cmd, "boot") != 0 || ush_streq(cmd, "bootargs") != 0) {
        return control_boot();
    }
    control_usage();
    return 0;
}

int cleonos_app_main(void) {
    char arg[USH_ARG_MAX];
    u64 argc;
    u64 i;
    int ok;

    arg[0] = '\0';
    argc = cleonos_sys_proc_argc();
    for (i = 1ULL; i < argc; i++) {
        char part[80];
        (void)cleonos_sys_proc_argv(i, part, (u64)sizeof(part));
        if (arg[0] != '\0') {
            strncat(arg, " ", sizeof(arg) - strlen(arg) - 1U);
        }
        strncat(arg, part, sizeof(arg) - strlen(arg) - 1U);
    }
    ush_trim_line(arg);
    ok = control_run(arg);
    return ok != 0 ? 0 : 1;
}
