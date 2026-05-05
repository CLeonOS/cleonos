#ifndef CLEONOS_TCC_SYS_TIME_H
#define CLEONOS_TCC_SYS_TIME_H

typedef long suseconds_t;

struct timeval {
    long tv_sec;
    suseconds_t tv_usec;
};

int gettimeofday(struct timeval *tv, void *tz);

#endif
