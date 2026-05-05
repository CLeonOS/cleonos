#ifndef CLEONOS_BDT_UNISTD_H
#define CLEONOS_BDT_UNISTD_H

typedef __PTRDIFF_TYPE__ ssize_t;
typedef long off_t;

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int close(int fd);
ssize_t read(int fd, void *buf, unsigned long count);
ssize_t write(int fd, const void *buf, unsigned long count);
off_t lseek(int fd, off_t offset, int whence);
char *getcwd(char *buf, unsigned long size);
int unlink(const char *path);

#endif
