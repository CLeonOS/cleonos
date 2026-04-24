#include <cleonos_syscall.h>
#include <stdio.h>

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
    (void)fputs("[fdtest] begin", 1);
    (void)fputc('\n', 1);

    (void)fputs("[fdtest] stdout fputs line", 1);
    (void)fputc('\n', 1);

    (void)printf("[fdtest] stdout printf value=%d hex=%X\n", 42, 0x2A);
    (void)dprintf(1, "[fdtest] stdout dprintf line\n");
    (void)fprintf(1, "[fdtest] stdout fprintf line\n");

    (void)fputs("[fdtest] stdout split part A", 1);
    (void)fputs(" + part B", 1);
    (void)fputc('\n', 1);

    (void)dprintf(2, "[fdtest] stderr dprintf line\n");
    (void)fprintf(2, "[fdtest] stderr fprintf line\n");

    fdtest_tty_line("[fdtest] tty syscall line");
    fdtest_tty_line("[fdtest] end");
    return 0;
}
