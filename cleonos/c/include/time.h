#ifndef CLEONOS_LIBC_TIME_H
#define CLEONOS_LIBC_TIME_H

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000L

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t time(time_t *out_time);
clock_t clock(void);

#endif
