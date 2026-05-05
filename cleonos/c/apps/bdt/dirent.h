#ifndef CLEONOS_BDT_DIRENT_H
#define CLEONOS_BDT_DIRENT_H

#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4

typedef struct bdt_cleonos_dir DIR;

struct dirent {
    unsigned char d_type;
    char d_name[96];
};

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif
