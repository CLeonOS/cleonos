#include "fastfetch_cleonos.h"

#include <stdio.h>
#include <string.h>

void ff_port_copy(char *dst, unsigned long dst_size, const char *src);
int ff_port_read_file(const char *path, char *out, unsigned long out_size);
int ff_port_env_value(const char *name, char *out, unsigned long out_size);
void ff_port_format_bytes(char *out, unsigned long out_size, u64 bytes);
void ff_port_format_uptime(char *out, unsigned long out_size, u64 uptime_ms);
void ff_port_ipv4_text(char *out, unsigned long out_size, u64 ipv4_be);

static int ff_port_parse_os_kv(const char *data, const char *key, char *out, unsigned long out_size) {
    unsigned long key_len;
    const char *p;

    if (data == (const char *)0 || key == (const char *)0 || out == (char *)0 || out_size == 0UL) {
        return 0;
    }

    key_len = (unsigned long)strlen(key);
    p = data;
    while (*p != '\0') {
        const char *line = p;
        const char *value;
        unsigned long len = 0UL;
        unsigned long at = 0UL;

        while (p[len] != '\0' && p[len] != '\n' && p[len] != '\r') {
            len++;
        }
        if (len > key_len && strncmp(line, key, (size_t)key_len) == 0 && line[key_len] == '=') {
            value = line + key_len + 1UL;
            if (*value == '"') {
                value++;
                while (at + 1UL < out_size && *value != '\0' && *value != '"' && *value != '\n' && *value != '\r') {
                    out[at++] = *value++;
                }
            } else {
                while (at + 1UL < out_size && *value != '\0' && *value != '\n' && *value != '\r') {
                    out[at++] = *value++;
                }
            }
            out[at] = '\0';
            return 1;
        }
        p += len;
        while (*p == '\n' || *p == '\r') {
            p++;
        }
    }
    return 0;
}

static void ff_port_load_os_version(ff_port_context *ctx) {
    char data[1024];

    ff_port_copy(ctx->os_pretty, sizeof(ctx->os_pretty), "CLeonOS");
    ff_port_copy(ctx->os_id, sizeof(ctx->os_id), "cleonos");
    ff_port_copy(ctx->os_version, sizeof(ctx->os_version), "0.1");
    data[0] = '\0';

    if (ff_port_read_file("/etc/os-version", data, sizeof(data)) == 0) {
        (void)ff_port_read_file("/etc/os-release", data, sizeof(data));
    }

    (void)ff_port_parse_os_kv(data, "PRETTY_NAME", ctx->os_pretty, sizeof(ctx->os_pretty));
    (void)ff_port_parse_os_kv(data, "ID", ctx->os_id, sizeof(ctx->os_id));
    (void)ff_port_parse_os_kv(data, "VERSION_ID", ctx->os_version, sizeof(ctx->os_version));
}

static void ff_port_load_shell(ff_port_context *ctx) {
    char value[FF_PORT_TEXT_MAX];

    if (ff_port_env_value("SHELL", value, sizeof(value)) != 0) {
        ff_port_copy(ctx->shell, sizeof(ctx->shell), value);
        return;
    }
    if (ff_port_env_value("LAUNCHER", value, sizeof(value)) != 0) {
        ff_port_copy(ctx->shell, sizeof(ctx->shell), value);
        return;
    }
    ff_port_copy(ctx->shell, sizeof(ctx->shell), "/shell/shell.elf");
}

static void ff_port_load_user(ff_port_context *ctx) {
    (void)memset(&ctx->user, 0, sizeof(ctx->user));
    if (cleonos_sys_user_current(&ctx->user) == 0ULL || ctx->user.name[0] == '\0') {
        ff_port_copy(ctx->user.name, sizeof(ctx->user.name), "user");
        ff_port_copy(ctx->user.home, sizeof(ctx->user.home), "/");
        ctx->user.logged_in = 0ULL;
    }
    ff_port_copy(ctx->home, sizeof(ctx->home), ctx->user.home[0] != '\0' ? ctx->user.home : "/");
}

static void ff_port_load_terminal(ff_port_context *ctx) {
    u64 active = cleonos_sys_tty_active();
    u64 count = cleonos_sys_tty_count();
    cleonos_tty_grid_info grid;

    (void)memset(&grid, 0, sizeof(grid));
    (void)cleonos_sys_tty_grid_info(&grid);
    (void)snprintf(ctx->terminal_text, sizeof(ctx->terminal_text), "TTY%llu (%llux%llu grid, %llu total)",
                   (unsigned long long)active, (unsigned long long)grid.cols, (unsigned long long)grid.rows,
                   (unsigned long long)count);
}

static void ff_port_load_resolution(ff_port_context *ctx) {
    cleonos_display_info tty_info;

    (void)memset(&tty_info, 0, sizeof(tty_info));
    if (cleonos_sys_display_info(CLEONOS_DISPLAY_TARGET_TTY, &tty_info) != 0ULL && tty_info.logical_width != 0ULL) {
        (void)snprintf(ctx->resolution_text, sizeof(ctx->resolution_text), "%llux%llu logical, %llux%llu physical",
                       (unsigned long long)tty_info.logical_width, (unsigned long long)tty_info.logical_height,
                       (unsigned long long)tty_info.physical_width, (unsigned long long)tty_info.physical_height);
    } else {
        ff_port_copy(ctx->resolution_text, sizeof(ctx->resolution_text), "unknown");
    }
}

static void ff_port_load_network(ff_port_context *ctx) {
    char ip[32];
    char gateway[32];
    char dns[32];

    if (cleonos_sys_net_available() == 0ULL) {
        ff_port_copy(ctx->network_text, sizeof(ctx->network_text), "offline");
        return;
    }
    ff_port_ipv4_text(ip, sizeof(ip), cleonos_sys_net_ipv4_addr());
    ff_port_ipv4_text(gateway, sizeof(gateway), cleonos_sys_net_gateway());
    ff_port_ipv4_text(dns, sizeof(dns), cleonos_sys_net_dns_server());
    (void)snprintf(ctx->network_text, sizeof(ctx->network_text), "%s, gw %s, dns %s", ip, gateway, dns);
}

void ff_port_context_init(ff_port_context *ctx) {
    char used[48];
    char total[48];
    u64 total_mem;
    u64 used_mem;

    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->config.logo = "cleonos";

    (void)cleonos_sys_sysinfo(&ctx->sysinfo);
    ff_port_load_os_version(ctx);
    ff_port_load_shell(ctx);
    ff_port_load_user(ctx);
    ff_port_load_terminal(ctx);
    ff_port_load_resolution(ctx);
    ff_port_load_network(ctx);

    if (cleonos_sys_locale_get(ctx->locale, (u64)sizeof(ctx->locale)) == 0ULL || ctx->locale[0] == '\0') {
        ff_port_copy(ctx->locale, sizeof(ctx->locale), "en_US");
    }

    (void)snprintf(ctx->kernel_text, sizeof(ctx->kernel_text), "%s %s",
                   ctx->sysinfo.kernel_name[0] != '\0' ? ctx->sysinfo.kernel_name : "CLKS",
                   ctx->sysinfo.kernel_version[0] != '\0' ? ctx->sysinfo.kernel_version : "unknown");
    ff_port_format_uptime(ctx->uptime_text, sizeof(ctx->uptime_text), ctx->sysinfo.uptime_ms);

    total_mem = ctx->sysinfo.managed_pages * 4096ULL;
    used_mem = ctx->sysinfo.used_pages * 4096ULL;
    ff_port_format_bytes(used, sizeof(used), used_mem);
    ff_port_format_bytes(total, sizeof(total), total_mem);
    (void)snprintf(ctx->memory_text, sizeof(ctx->memory_text), "%s / %s", used, total);

    if (cleonos_sys_disk_present() != 0ULL) {
        char disk_size[48];
        char mount[96];
        ff_port_format_bytes(disk_size, sizeof(disk_size), cleonos_sys_disk_size_bytes());
        mount[0] = '\0';
        if (cleonos_sys_disk_mounted() != 0ULL) {
            (void)cleonos_sys_disk_mount_path(mount, (u64)sizeof(mount));
        }
        (void)snprintf(ctx->disk_text, sizeof(ctx->disk_text), "%s%s%s", disk_size, mount[0] ? " mounted at " : "",
                       mount[0] ? mount : "");
    } else {
        ff_port_copy(ctx->disk_text, sizeof(ctx->disk_text), "not present");
    }

    (void)snprintf(ctx->host_text, sizeof(ctx->host_text), "CLeonOS on %s",
                   ctx->sysinfo.arch[0] != '\0' ? ctx->sysinfo.arch : "x86_64");
    if (cleonos_sys_boot_cmdline(ctx->bootargs, (u64)sizeof(ctx->bootargs)) == 0ULL) {
        ctx->bootargs[0] = '\0';
    }
}
