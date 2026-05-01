#ifndef CLEONOS_DRIVER_STUB_H
#define CLEONOS_DRIVER_STUB_H

#include <cleonos_syscall.h>

static int cleonos_driver_stub_main(const char *banner) {
    if (banner != (const char *)0) {
        u64 len = 0ULL;

        while (banner[len] != '\0') {
            len++;
        }

        (void)cleonos_sys_log_write(banner, len);
    }

    return 0;
}

#endif
