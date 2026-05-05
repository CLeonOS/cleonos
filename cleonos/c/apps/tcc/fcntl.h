#ifndef CLEONOS_TCC_FCNTL_H
#define CLEONOS_TCC_FCNTL_H

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_BINARY 0

int open(const char *path, int flags, ...);

#endif
