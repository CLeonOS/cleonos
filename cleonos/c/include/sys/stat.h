#ifndef CLEONOS_LIBC_SYS_STAT_H
#define CLEONOS_LIBC_SYS_STAT_H

#include <stddef.h>
#include <time.h>

#include <sys/types.h>

#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    mode_t st_mode;
    unsigned long st_nlink;
    unsigned long st_uid;
    unsigned long st_gid;
    unsigned long st_rdev;
    off_t st_size;
    long st_blksize;
    long st_blocks;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

int stat(const char *path, struct stat *out_stat);
int fstat(int fd, struct stat *out_stat);

#endif
