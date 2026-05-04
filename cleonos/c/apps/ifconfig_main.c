#include "cmd_runtime.h"
#include <stdio.h>

static void ush_ifconfig_print_ipv4(u64 ipv4_be) {
    unsigned int a = (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL);
    unsigned int b = (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL);
    unsigned int c = (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL);
    unsigned int d = (unsigned int)(ipv4_be & 0xFFULL);

    (void)printf("%u.%u.%u.%u", a, b, c, d);
}

static int ush_cmd_ifconfig(const char *arg) {
    u64 available;
    u64 ip;
    u64 netmask;
    u64 gateway;
    u64 dns;

    if (arg != (const char *)0 && arg[0] != '\0') {
        if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
            ush_writeln_i18n("usage: ifconfig", "用法: ifconfig");
            return 1;
        }
        ush_writeln_i18n("ifconfig: usage ifconfig", "ifconfig: 用法 ifconfig");
        return 0;
    }

    available = cleonos_sys_net_available();
    if (available == 0ULL) {
        ush_writeln_i18n("ifconfig: network unavailable", "ifconfig: 网络不可用");
        return 0;
    }

    ip = cleonos_sys_net_ipv4_addr();
    netmask = cleonos_sys_net_netmask();
    gateway = cleonos_sys_net_gateway();
    dns = cleonos_sys_net_dns_server();

    ush_write_i18n_label("inet", "IPv4 地址");
    ush_write(": ");
    ush_ifconfig_print_ipv4(ip);
    (void)putchar('\n');

    ush_write_i18n_label("netmask", "子网掩码");
    ush_write(": ");
    ush_ifconfig_print_ipv4(netmask);
    (void)putchar('\n');

    ush_write_i18n_label("gateway", "网关");
    ush_write(": ");
    ush_ifconfig_print_ipv4(gateway);
    (void)putchar('\n');

    ush_write_i18n_label("dns", "DNS");
    ush_write(": ");
    ush_ifconfig_print_ipv4(dns);
    (void)putchar('\n');

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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ifconfig") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ifconfig(arg);

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
