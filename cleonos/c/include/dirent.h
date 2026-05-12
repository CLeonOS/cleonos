#ifndef CLEONOS_LIBC_DIRENT_H
#define CLEONOS_LIBC_DIRENT_H

#include <cleonos_syscall.h>

#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4

struct dirent {
    unsigned long d_ino;
    unsigned char d_type;
    char d_name[CLEONOS_FS_NAME_MAX];
};

typedef struct DIR DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif
