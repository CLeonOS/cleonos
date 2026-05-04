#ifndef CLEONOS_LUA_TIME_H
#define CLEONOS_LUA_TIME_H

typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000L

time_t time(time_t *out_time);
clock_t clock(void);

#endif
