#include <cleonos_syscall.h>
#include <time.h>

time_t time(time_t *out_time) {
    time_t seconds = (time_t)(cleonos_sys_time_ms() / 1000ULL);

    if (out_time != (time_t *)0) {
        *out_time = seconds;
    }

    return seconds;
}

clock_t clock(void) {
    return (clock_t)cleonos_sys_time_ms();
}
