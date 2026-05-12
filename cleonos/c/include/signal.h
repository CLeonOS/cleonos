#ifndef CLEONOS_LIBC_SIGNAL_H
#define CLEONOS_LIBC_SIGNAL_H

typedef int sigset_t;
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGINT 2
#define SIGQUIT 3
#define SIGTERM 15
#define SIGCHLD 17
#define SIG_BLOCK 0

struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
};

int sigemptyset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

#endif
