#ifndef CLEONOS_BDT_SYS_STAT_H
#define CLEONOS_BDT_SYS_STAT_H

typedef unsigned long mode_t;

#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

struct stat {
    mode_t st_mode;
    unsigned long st_size;
    long st_mtime;
};

int stat(const char *path, struct stat *out);
int mkdir(const char *path, mode_t mode);

#endif
