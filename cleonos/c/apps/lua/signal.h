#ifndef CLEONOS_LUA_SIGNAL_H
#define CLEONOS_LUA_SIGNAL_H

typedef int sig_atomic_t;

#define SIGINT 2
#define SIGTERM 15
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int))-1)

void (*signal(int sig, void (*handler)(int)))(int);

#endif
