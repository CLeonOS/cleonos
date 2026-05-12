#ifndef CLEONOS_LIBC_TIME_H
#define CLEONOS_LIBC_TIME_H

typedef long time_t;
typedef long clock_t;

typedef unsigned long size_t;

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
struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *timer, struct tm *out_tm);
struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *timer, struct tm *out_tm);
time_t mktime(struct tm *tm_value);
size_t strftime(char *out, size_t out_size, const char *format, const struct tm *tm_value);

#endif
