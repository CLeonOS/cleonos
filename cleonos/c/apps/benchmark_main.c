#include "cmd_runtime.h"

#define BENCH_TMP_PATH "/temp/benchmark.tmp"
#define BENCH_MEM_SIZE 65536ULL
#define BENCH_FILE_SIZE 65536ULL

typedef struct bench_mode {
    const char *name;
    u64 cpu_iters;
    u64 mem_rounds;
    u64 syscall_iters;
    u64 file_rounds;
} bench_mode;

typedef struct bench_result {
    const char *name;
    u64 work_units;
    const char *unit_name;
    u64 ticks;
    u64 rate_per_tick;
    u64 checksum;
    int ok;
} bench_result;

static unsigned char bench_src[BENCH_MEM_SIZE];
static unsigned char bench_dst[BENCH_MEM_SIZE];
static char bench_file_buf[BENCH_FILE_SIZE];

static void bench_print_help(void) {
    ush_writeln_i18n("usage: benchmark [--quick|--full|--help]", "用法: benchmark [--quick|--full|--help]");
    puts("");
    ush_writeln_i18n("Runs simple userland speed tests:", "运行简单用户态测速:");
    ush_writeln_i18n("  cpu      integer xorshift loop", "  cpu      整数 xorshift 循环");
    ush_writeln_i18n("  memory   fill/copy/checksum over static buffers", "  memory   静态缓冲区填充/复制/校验");
    ush_writeln_i18n("  syscall  timer syscall throughput", "  syscall  timer 系统调用吞吐");
    ush_writeln_i18n("  file     /temp write/read/remove throughput", "  file     /temp 写入/读取/删除吞吐");
}

static u64 bench_elapsed_ticks(u64 start, u64 end) {
    if (end <= start) {
        return 1ULL;
    }
    return end - start;
}

static u64 bench_div_nonzero(u64 value, u64 denom) {
    if (denom == 0ULL) {
        return value;
    }
    return value / denom;
}

static u64 bench_u64_to_dec(char *out, u64 out_size, u64 value) {
    char rev[32];
    u64 digits = 0ULL;
    u64 i;

    if (out == (char *)0 || out_size == 0ULL) {
        return 0ULL;
    }

    if (value == 0ULL) {
        if (out_size < 2ULL) {
            return 0ULL;
        }
        out[0] = '0';
        out[1] = '\0';
        return 1ULL;
    }

    while (value > 0ULL && digits < (u64)sizeof(rev)) {
        rev[digits++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }

    if (digits + 1ULL > out_size) {
        out[0] = '\0';
        return 0ULL;
    }

    for (i = 0ULL; i < digits; i++) {
        out[i] = rev[digits - 1ULL - i];
    }
    out[digits] = '\0';
    return digits;
}

static void bench_print_u64_width(u64 value, u64 width) {
    char text[32];
    u64 len;
    u64 i;

    len = bench_u64_to_dec(text, (u64)sizeof(text), value);
    if (len == 0ULL) {
        len = 1ULL;
        text[0] = '0';
        text[1] = '\0';
    }

    if (width > len) {
        for (i = 0ULL; i < width - len; i++) {
            putchar(' ');
        }
    }

    (void)fputs(text, 1);
}

static void bench_print_result(const bench_result *result) {
    if (result == (const bench_result *)0) {
        return;
    }

    printf("%-8s ", result->name);

    if (result->ok == 0) {
        ush_writeln_i18n("skipped/failed", "已跳过/失败");
        return;
    }

    bench_print_u64_width(result->work_units, 12ULL);
    printf(" %-8s ", result->unit_name);
    bench_print_u64_width(result->ticks, 8ULL);
    printf(" ticks  ");
    bench_print_u64_width(result->rate_per_tick, 12ULL);
    printf(" %s/tick  checksum=0x%llX\n", result->unit_name, (unsigned long long)result->checksum);
}

static bench_result bench_run_cpu(const bench_mode *mode) {
    bench_result result;
    volatile u64 state = 0xC1E0C1E0D00D1234ULL;
    u64 start;
    u64 end;
    u64 i;

    result.name = "cpu";
    result.work_units = mode->cpu_iters;
    result.unit_name = "ops";
    result.ticks = 0ULL;
    result.rate_per_tick = 0ULL;
    result.checksum = 0ULL;
    result.ok = 1;

    start = cleonos_sys_timer_ticks();
    for (i = 0ULL; i < mode->cpu_iters; i++) {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        state += (i * 0x9E3779B97F4A7C15ULL) ^ 0xA5A5A5A5A5A5A5A5ULL;
    }
    end = cleonos_sys_timer_ticks();

    result.ticks = bench_elapsed_ticks(start, end);
    result.rate_per_tick = bench_div_nonzero(result.work_units, result.ticks);
    result.checksum = state;
    return result;
}

static bench_result bench_run_memory(const bench_mode *mode) {
    bench_result result;
    volatile u64 checksum = 0ULL;
    u64 start;
    u64 end;
    u64 round;
    u64 i;

    result.name = "memory";
    result.work_units = BENCH_MEM_SIZE * mode->mem_rounds * 2ULL;
    result.unit_name = "bytes";
    result.ticks = 0ULL;
    result.rate_per_tick = 0ULL;
    result.checksum = 0ULL;
    result.ok = 1;

    start = cleonos_sys_timer_ticks();
    for (round = 0ULL; round < mode->mem_rounds; round++) {
        for (i = 0ULL; i < BENCH_MEM_SIZE; i++) {
            bench_src[i] = (unsigned char)((i + round) & 0xFFULL);
        }

        memcpy(bench_dst, bench_src, (unsigned long)BENCH_MEM_SIZE);

        for (i = 0ULL; i < BENCH_MEM_SIZE; i += 64ULL) {
            checksum += (u64)bench_dst[i];
        }
    }
    end = cleonos_sys_timer_ticks();

    result.ticks = bench_elapsed_ticks(start, end);
    result.rate_per_tick = bench_div_nonzero(result.work_units, result.ticks);
    result.checksum = checksum;
    return result;
}

static bench_result bench_run_syscall(const bench_mode *mode) {
    bench_result result;
    volatile u64 checksum = 0ULL;
    u64 start;
    u64 end;
    u64 i;

    result.name = "syscall";
    result.work_units = mode->syscall_iters;
    result.unit_name = "calls";
    result.ticks = 0ULL;
    result.rate_per_tick = 0ULL;
    result.checksum = 0ULL;
    result.ok = 1;

    start = cleonos_sys_timer_ticks();
    for (i = 0ULL; i < mode->syscall_iters; i++) {
        checksum ^= cleonos_sys_getpid();
        checksum += cleonos_sys_timer_ticks();
    }
    end = cleonos_sys_timer_ticks();

    result.ticks = bench_elapsed_ticks(start, end);
    result.rate_per_tick = bench_div_nonzero(result.work_units * 2ULL, result.ticks);
    result.work_units *= 2ULL;
    result.checksum = checksum;
    return result;
}

static bench_result bench_run_file(const bench_mode *mode) {
    bench_result result;
    volatile u64 checksum = 0ULL;
    u64 start;
    u64 end;
    u64 round;
    u64 i;

    result.name = "file";
    result.work_units = BENCH_FILE_SIZE * mode->file_rounds * 2ULL;
    result.unit_name = "bytes";
    result.ticks = 0ULL;
    result.rate_per_tick = 0ULL;
    result.checksum = 0ULL;
    result.ok = 1;

    for (i = 0ULL; i < BENCH_FILE_SIZE; i++) {
        bench_file_buf[i] = (char)('A' + (i % 26ULL));
    }

    start = cleonos_sys_timer_ticks();
    for (round = 0ULL; round < mode->file_rounds; round++) {
        u64 got;

        if (cleonos_sys_fs_write(BENCH_TMP_PATH, bench_file_buf, BENCH_FILE_SIZE) == 0ULL) {
            result.ok = 0;
            break;
        }

        memset(bench_file_buf, 0, (unsigned long)BENCH_FILE_SIZE);
        got = cleonos_sys_fs_read(BENCH_TMP_PATH, bench_file_buf, BENCH_FILE_SIZE);
        if (got == 0ULL) {
            result.ok = 0;
            break;
        }

        for (i = 0ULL; i < got; i += 128ULL) {
            checksum += (u64)(unsigned char)bench_file_buf[i];
        }
    }
    end = cleonos_sys_timer_ticks();

    (void)cleonos_sys_fs_remove(BENCH_TMP_PATH);

    if (result.ok == 0) {
        return result;
    }

    result.ticks = bench_elapsed_ticks(start, end);
    result.rate_per_tick = bench_div_nonzero(result.work_units, result.ticks);
    result.checksum = checksum;
    return result;
}

static int bench_select_mode(const char *arg, bench_mode *out_mode) {
    if (out_mode == (bench_mode *)0) {
        return 0;
    }

    out_mode->name = "default";
    out_mode->cpu_iters = 500000ULL;
    out_mode->mem_rounds = 64ULL;
    out_mode->syscall_iters = 2000ULL;
    out_mode->file_rounds = 4ULL;

    if (arg == (const char *)0 || arg[0] == '\0') {
        return 1;
    }

    if (ush_streq(arg, "--quick") != 0 || ush_streq(arg, "-q") != 0) {
        out_mode->name = "quick";
        out_mode->cpu_iters = 100000ULL;
        out_mode->mem_rounds = 16ULL;
        out_mode->syscall_iters = 500ULL;
        out_mode->file_rounds = 1ULL;
        return 1;
    }

    if (ush_streq(arg, "--full") != 0 || ush_streq(arg, "-f") != 0) {
        out_mode->name = "full";
        out_mode->cpu_iters = 2000000ULL;
        out_mode->mem_rounds = 256ULL;
        out_mode->syscall_iters = 10000ULL;
        out_mode->file_rounds = 16ULL;
        return 1;
    }

    if (ush_streq(arg, "--help") != 0 || ush_streq(arg, "-h") != 0) {
        bench_print_help();
        return 2;
    }

    ush_writeln_i18n("benchmark: usage benchmark [--quick|--full|--help]",
                     "benchmark: 用法 benchmark [--quick|--full|--help]");
    return 0;
}

static int ush_cmd_benchmark(const char *arg) {
    bench_mode mode;
    bench_result cpu;
    bench_result memory;
    bench_result syscall;
    bench_result file;
    u64 total_ticks;
    int selected;

    selected = bench_select_mode(arg, &mode);
    if (selected == 0) {
        return 0;
    }
    if (selected == 2) {
        return 1;
    }

    (void)printf((ush_locale_is_zh() != 0) ? "CLeonOS benchmark 测速 (%s)\n" : "CLeonOS benchmark (%s)\n",
                 mode.name);
    ush_writeln_i18n("test     work         unit     elapsed   throughput      checksum",
                     "test     work         unit     elapsed   throughput      checksum");
    puts("------------------------------------------------------------------");

    total_ticks = cleonos_sys_timer_ticks();
    cpu = bench_run_cpu(&mode);
    memory = bench_run_memory(&mode);
    syscall = bench_run_syscall(&mode);
    file = bench_run_file(&mode);
    total_ticks = bench_elapsed_ticks(total_ticks, cleonos_sys_timer_ticks());

    bench_print_result(&cpu);
    bench_print_result(&memory);
    bench_print_result(&syscall);
    bench_print_result(&file);

    puts("------------------------------------------------------------------");
    (void)printf((ush_locale_is_zh() != 0) ? "total (总计): %llu ticks\n" : "total: %llu ticks\n",
                 (unsigned long long)total_ticks);
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
        if (ctx.cmd[0] != '\0' && ush_streq(ctx.cmd, "benchmark") != 0) {
            has_context = 1;
            arg = ctx.arg;
            if (ctx.cwd[0] == '/') {
                ush_copy(sh.cwd, (u64)sizeof(sh.cwd), ctx.cwd);
                ush_copy(initial_cwd, (u64)sizeof(initial_cwd), sh.cwd);
            }
        }
    }

    success = ush_cmd_benchmark(arg);

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
