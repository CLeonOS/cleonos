#ifndef CLEONOS_BDT_ERRNO_H
#define CLEONOS_BDT_ERRNO_H

#define EPERM 1
#define ENOENT 2
#define EIO 5
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define EMFILE 24
#define ESPIPE 29
#define ERANGE 34
#define ENOSYS 38

extern int errno;

int *__errno_location(void);
char *strerror(int errnum);

#endif
