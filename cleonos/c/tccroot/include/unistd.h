#ifndef CLEONOS_TCCROOT_UNISTD_H
#define CLEONOS_TCCROOT_UNISTD_H

#include <stddef.h>

typedef long ssize_t;
typedef long off_t;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int access(const char *path, int mode);
int unlink(const char *path);

#endif
