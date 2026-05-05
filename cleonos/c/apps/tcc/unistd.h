#ifndef CLEONOS_TCC_UNISTD_H
#define CLEONOS_TCC_UNISTD_H

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef __PTRDIFF_TYPE__ ssize_t;
#endif

#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long off_t;
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int close(int fd);
ssize_t read(int fd, void *buf, unsigned long count);
ssize_t write(int fd, const void *buf, unsigned long count);
off_t lseek(int fd, off_t offset, int whence);
int access(const char *path, int mode);
char *getcwd(char *buf, unsigned long size);
int unlink(const char *path);
int execvp(const char *file, char *const argv[]);

#endif
