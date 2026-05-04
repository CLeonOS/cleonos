#include "cmd_runtime.h"
#include <stdio.h>

#define USH_PING_POLL_BUDGET 200000000ULL

static int ush_ping_parse_ipv4_be(const char *text, u64 *out_ipv4_be) {
    u64 acc = 0ULL;
    u64 value = 0ULL;
    u64 parts = 0ULL;
    u64 has_digit = 0ULL;
    u64 i = 0ULL;

    if (text == (const char *)0 || out_ipv4_be == (u64 *)0) {
        return 0;
    }

    while (text[i] != '\0') {
        char ch = text[i];

        if (ch >= '0' && ch <= '9') {
            value = (value * 10ULL) + (u64)(ch - '0');
            if (value > 255ULL) {
                return 0;
            }
            has_digit = 1ULL;
        } else if (ch == '.') {
            if (has_digit == 0ULL || parts >= 3ULL) {
                return 0;
            }
            acc = (acc << 8ULL) | value;
            parts++;
            value = 0ULL;
            has_digit = 0ULL;
        } else {
            return 0;
        }

        i++;
    }

    if (has_digit == 0ULL || parts != 3ULL) {
        return 0;
    }

    acc = (acc << 8ULL) | value;
    *out_ipv4_be = acc & 0xFFFFFFFFULL;
    return 1;
}

static void ush_ping_print_ipv4(u64 ipv4_be) {
    unsigned int a = (unsigned int)((ipv4_be >> 24ULL) & 0xFFULL);
    unsigned int b = (unsigned int)((ipv4_be >> 16ULL) & 0xFFULL);
    unsigned int c = (unsigned int)((ipv4_be >> 8ULL) & 0xFFULL);
    unsigned int d = (unsigned int)(ipv4_be & 0xFFULL);

    (void)printf("%u.%u.%u.%u", a, b, c, d);
}

static int ush_cmd_ping(const char *arg) {
    char host[USH_PATH_MAX];
    char count_text[32];
    const char *rest = "";
    const char *rest2 = "";
    u64 target_ipv4_be = 0ULL;
    u64 local_ipv4_be;
    u64 count = 4ULL;
    u64 i;
    u64 received = 0ULL;
    int zh = ush_locale_is_zh();

    if (arg == (const char *)0 || arg[0] == '\0') {
        ush_writeln_i18n("ping: usage ping <a.b.c.d> [count]", "ping: 用法 ping <a.b.c.d> [count]");
        return 0;
    }

    if (ush_split_first_and_rest(arg, host, (u64)sizeof(host), &rest) == 0) {
        ush_writeln_i18n("ping: usage ping <a.b.c.d> [count]", "ping: 用法 ping <a.b.c.d> [count]");
        return 0;
    }

    if (ush_ping_parse_ipv4_be(host, &target_ipv4_be) == 0) {
        ush_writeln_i18n("ping: invalid ipv4 address", "ping: 无效 IPv4 地址");
        return 0;
    }

    if (rest != (const char *)0 && rest[0] != '\0') {
        if (ush_split_first_and_rest(rest, count_text, (u64)sizeof(count_text), &rest2) == 0) {
            ush_writeln_i18n("ping: usage ping <a.b.c.d> [count]", "ping: 用法 ping <a.b.c.d> [count]");
            return 0;
        }

        if (rest2 != (const char *)0 && rest2[0] != '\0') {
            ush_writeln_i18n("ping: usage ping <a.b.c.d> [count]", "ping: 用法 ping <a.b.c.d> [count]");
            return 0;
        }

        if (ush_parse_u64_dec(count_text, &count) == 0 || count == 0ULL || count > 64ULL) {
            ush_writeln_i18n("ping: count must be in range 1..64", "ping: count 必须在 1..64 范围内");
            return 0;
        }
    }

    if (cleonos_sys_net_available() == 0ULL) {
        ush_writeln_i18n("ping: network unavailable (e1000 not ready)", "ping: 网络不可用 (e1000 未就绪)");
        return 0;
    }

    local_ipv4_be = cleonos_sys_net_ipv4_addr();

    (void)fputs((zh != 0) ? "PING 目标 " : "PING ", 1);
    ush_ping_print_ipv4(target_ipv4_be);
    (void)fputs((zh != 0) ? " 来自 " : " from ", 1);
    ush_ping_print_ipv4(local_ipv4_be);
    (void)putchar('\n');

    for (i = 0ULL; i < count; i++) {
        u64 ok = cleonos_sys_net_ping(target_ipv4_be, USH_PING_POLL_BUDGET);

        if (ok != 0ULL) {
            (void)fputs((zh != 0) ? "收到回复 来自 " : "reply from ", 1);
            ush_ping_print_ipv4(target_ipv4_be);
            (void)printf(" seq=%llu\n", (unsigned long long)(i + 1ULL));
            received++;
        } else {
            (void)fputs((zh != 0) ? "超时 来自 " : "timeout from ", 1);
            ush_ping_print_ipv4(target_ipv4_be);
            (void)printf(" seq=%llu\n", (unsigned long long)(i + 1ULL));
        }

        if (i + 1ULL < count) {
            (void)cleonos_sys_sleep_ticks(30ULL);
        }
    }

    (void)puts("");
    (void)printf((zh != 0) ? "ping 统计 (ping stats): tx=%llu rx=%llu loss=%llu\n"
                           : "ping stats: tx=%llu rx=%llu loss=%llu\n",
                 (unsigned long long)count, (unsigned long long)received, (unsigned long long)(count - received));

    return (received > 0ULL) ? 1 : 0;
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "ping") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_ping(arg);

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
