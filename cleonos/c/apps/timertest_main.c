#include "cmd_runtime.h"

#include <stdio.h>

static u64 timertest_elapsed(u64 before, u64 after) {
    return (after >= before) ? (after - before) : 0ULL;
}

static void timertest_print_delta(const char *name, u64 before_tick, u64 after_tick, u64 before_ms, u64 after_ms) {
    printf("%s: ticks=%llu ms=%llu\n", name, (unsigned long long)timertest_elapsed(before_tick, after_tick),
           (unsigned long long)timertest_elapsed(before_ms, after_ms));
}

static int timertest_run(void) {
    u64 hz;
    u64 tick0;
    u64 tick1;
    u64 ms0;
    u64 ms1;
    u64 slept;
    u64 i;
    int monotonic_ok = 1;

    hz = cleonos_sys_timer_hz();
    if (hz == 0ULL || hz == (u64)-1) {
        ush_writeln_i18n("timertest: timer_hz unavailable", "timertest: timer_hz 不可用");
        return 0;
    }

    tick0 = cleonos_sys_timer_ticks();
    ms0 = cleonos_sys_time_ms();

    (void)printf((ush_locale_is_zh() != 0) ? "timer_hz (计时器频率): %llu\n" : "timer_hz: %llu\n",
                 (unsigned long long)hz);
    (void)printf((ush_locale_is_zh() != 0) ? "start_ticks (起始Ticks): %llu\n" : "start_ticks: %llu\n",
                 (unsigned long long)tick0);
    (void)printf((ush_locale_is_zh() != 0) ? "start_ms (起始毫秒): %llu\n" : "start_ms: %llu\n",
                 (unsigned long long)ms0);

    ush_writeln_i18n("test: sleep_ms(1000)", "测试 (test): sleep_ms(1000)");
    tick0 = cleonos_sys_timer_ticks();
    ms0 = cleonos_sys_time_ms();
    slept = cleonos_sys_sleep_ms(1000ULL);
    tick1 = cleonos_sys_timer_ticks();
    ms1 = cleonos_sys_time_ms();
    (void)printf((ush_locale_is_zh() != 0) ? "sleep_ms_return (返回值): %llu\n" : "sleep_ms_return: %llu\n",
                 (unsigned long long)slept);
    timertest_print_delta("sleep_ms_delta", tick0, tick1, ms0, ms1);

    ush_writeln_i18n("test: sleep_ticks(timer_hz)", "测试 (test): sleep_ticks(timer_hz)");
    tick0 = cleonos_sys_timer_ticks();
    ms0 = cleonos_sys_time_ms();
    slept = cleonos_sys_sleep_ticks(hz);
    tick1 = cleonos_sys_timer_ticks();
    ms1 = cleonos_sys_time_ms();
    (void)printf((ush_locale_is_zh() != 0) ? "sleep_ticks_return (返回值): %llu\n" : "sleep_ticks_return: %llu\n",
                 (unsigned long long)slept);
    timertest_print_delta("sleep_ticks_delta", tick0, tick1, ms0, ms1);

    ush_writeln_i18n("test: time_ms monotonic sample", "测试 (test): time_ms 单调性采样");
    ms0 = cleonos_sys_time_ms();
    for (i = 0ULL; i < 64ULL; i++) {
        ms1 = cleonos_sys_time_ms();
        if (ms1 < ms0) {
            monotonic_ok = 0;
            break;
        }
        ms0 = ms1;
        (void)cleonos_sys_sleep_ms(10ULL);
    }

    (void)printf((ush_locale_is_zh() != 0) ? "monotonic_ms (毫秒单调性): %s\n" : "monotonic_ms: %s\n",
                 (monotonic_ok != 0) ? "ok" : "failed");
    return monotonic_ok;
}

int cleonos_app_main(void) {
    ush_cmd_ctx ctx;
    ush_cmd_ret ret;
    ush_state sh;
    char initial_cwd[USH_PATH_MAX];
    int has_context = 0;
    int success;

    ush_zero(&ctx, (u64)sizeof(ctx));
    ush_zero(&ret, (u64)sizeof(ret));
    ush_init_state(&sh);
    ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);

    if (ush_command_ctx_read(&ctx) != 0) {
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "timertest") != 0) {
            has_context = 1;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = timertest_run();

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
