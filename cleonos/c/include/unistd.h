#ifndef CLEONOS_LIBC_UNISTD_H
#define CLEONOS_LIBC_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 84

ssize_t read(int fd, void *buffer, size_t size);
ssize_t write(int fd, const void *buffer, size_t size);
int close(int fd);
int dup(int fd);
int isatty(int fd);
int fileno(int fd);
pid_t getpid(void);
long sysconf(int name);
char *getcwd(char *buffer, size_t size);
int chdir(const char *path);

#endif
