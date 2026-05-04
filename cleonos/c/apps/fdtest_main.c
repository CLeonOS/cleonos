#include <cleonos_syscall.h>
#include <stdio.h>
#include "cmd_runtime.h"

static void fdtest_tty_line(const char *text) {
    u64 len = 0ULL;

    if (text == (const char *)0) {
        return;
    }

    while (text[len] != '\0') {
        len++;
    }

    (void)cleonos_sys_tty_write(text, len);
    (void)cleonos_sys_tty_write("\n", 1ULL);
}

int cleonos_app_main(void) {
    (void)fputs((ush_locale_is_zh() != 0) ? "[fdtest] 开始 (begin)" : "[fdtest] begin", 1);
    (void)fputc('\n', 1);

    (void)fputs((ush_locale_is_zh() != 0) ? "[fdtest] stdout fputs 行 (line)" : "[fdtest] stdout fputs line", 1);
    (void)fputc('\n', 1);

    (void)printf((ush_locale_is_zh() != 0) ? "[fdtest] stdout printf 值 (value)=%d hex=%X\n"
                                           : "[fdtest] stdout printf value=%d hex=%X\n",
                 42, 0x2A);
    (void)dprintf(1, (ush_locale_is_zh() != 0) ? "[fdtest] stdout dprintf 行 (line)\n"
                                               : "[fdtest] stdout dprintf line\n");
    (void)fprintf(1, (ush_locale_is_zh() != 0) ? "[fdtest] stdout fprintf 行 (line)\n"
                                               : "[fdtest] stdout fprintf line\n");

    (void)fputs((ush_locale_is_zh() != 0) ? "[fdtest] stdout 分段 A (split part A)"
                                          : "[fdtest] stdout split part A",
                 1);
    (void)fputs((ush_locale_is_zh() != 0) ? " + 分段 B (part B)" : " + part B", 1);
    (void)fputc('\n', 1);

    (void)dprintf(2, (ush_locale_is_zh() != 0) ? "[fdtest] stderr dprintf 行 (line)\n"
                                               : "[fdtest] stderr dprintf line\n");
    (void)fprintf(2, (ush_locale_is_zh() != 0) ? "[fdtest] stderr fprintf 行 (line)\n"
                                               : "[fdtest] stderr fprintf line\n");

    fdtest_tty_line((ush_locale_is_zh() != 0) ? "[fdtest] tty syscall 行 (line)" : "[fdtest] tty syscall line");
    fdtest_tty_line((ush_locale_is_zh() != 0) ? "[fdtest] 结束 (end)" : "[fdtest] end");
    return 0;
}
