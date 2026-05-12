#ifndef CLEONOS_LIBC_FCNTL_H
#define CLEONOS_LIBC_FCNTL_H

#include <cleonos_syscall.h>

#define O_RDONLY ((int)CLEONOS_O_RDONLY)
#define O_WRONLY ((int)CLEONOS_O_WRONLY)
#define O_RDWR ((int)CLEONOS_O_RDWR)
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#define O_CREAT ((int)CLEONOS_O_CREAT)
#define O_TRUNC ((int)CLEONOS_O_TRUNC)
#define O_APPEND ((int)CLEONOS_O_APPEND)

int open(const char *path, int flags, ...);

#endif
