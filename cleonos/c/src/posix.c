#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cleonos_syscall.h>

__attribute__((weak)) int errno;

struct DIR {
    char path[192];
    unsigned long index;
    unsigned long count;
    struct dirent current;
};

static void posix_copy(char *dst, size_t dst_size, const char *src) {
    if (dst == (char *)0 || dst_size == 0U) {
        return;
    }
    if (src == (const char *)0) {
        dst[0] = '\0';
        return;
    }
    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

__attribute__((weak)) ssize_t read(int fd, void *buffer, size_t size) {
    u64 got;

    got = cleonos_sys_fd_read((u64)(unsigned int)fd, buffer, (u64)size);
    if (got == (u64)-1) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)got;
}

__attribute__((weak)) ssize_t write(int fd, const void *buffer, size_t size) {
    u64 written;

    written = cleonos_sys_fd_write((u64)(unsigned int)fd, buffer, (u64)size);
    if (written == (u64)-1) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)written;
}

__attribute__((weak)) int close(int fd) {
    if (cleonos_sys_fd_close((u64)(unsigned int)fd) == 0ULL) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

__attribute__((weak)) int dup(int fd) {
    u64 new_fd = cleonos_sys_fd_dup((u64)(unsigned int)fd);
    if (new_fd == (u64)-1 || new_fd == 0ULL) {
        errno = EBADF;
        return -1;
    }
    return (int)new_fd;
}

__attribute__((weak)) int open(const char *path, int flags, ...) {
    u64 fd;
    (void)flags;

    fd = cleonos_sys_fd_open(path, (u64)(unsigned int)flags, 0644ULL);
    if (fd == (u64)-1 || fd == 0ULL) {
        errno = ENOENT;
        return -1;
    }
    return (int)fd;
}

__attribute__((weak)) int isatty(int fd) {
    return (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) ? 1 : 0;
}

__attribute__((weak)) int fileno(int fd) {
    return fd;
}

__attribute__((weak)) pid_t getpid(void) {
    return (pid_t)cleonos_sys_getpid();
}

__attribute__((weak)) long sysconf(int name) {
    if (name == _SC_PAGESIZE || name == _SC_PAGE_SIZE) {
        return 4096L;
    }
    if (name == _SC_NPROCESSORS_ONLN) {
        return 1L;
    }
    errno = EINVAL;
    return -1L;
}

__attribute__((weak)) char *getcwd(char *buffer, size_t size) {
    if (buffer == (char *)0 || size == 0U) {
        errno = EINVAL;
        return (char *)0;
    }
    posix_copy(buffer, size, "/");
    return buffer;
}

__attribute__((weak)) int chdir(const char *path) {
    if (path == (const char *)0 || path[0] == '\0' || cleonos_sys_fs_stat_type(path) == 0ULL) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

__attribute__((weak)) int stat(const char *path, struct stat *out_stat) {
    u64 type;
    u64 size;

    if (path == (const char *)0 || out_stat == (struct stat *)0) {
        errno = EINVAL;
        return -1;
    }

    type = cleonos_sys_fs_stat_type(path);
    if (type == 0ULL) {
        errno = ENOENT;
        return -1;
    }

    size = cleonos_sys_fs_stat_size(path);
    (void)memset(out_stat, 0, sizeof(*out_stat));
    out_stat->st_mode = (type == 2ULL) ? (S_IFDIR | 0755UL) : (S_IFREG | 0644UL);
    out_stat->st_size = (off_t)size;
    out_stat->st_blksize = 4096L;
    out_stat->st_blocks = (long)((size + 511ULL) / 512ULL);
    return 0;
}

__attribute__((weak)) int fstat(int fd, struct stat *out_stat) {
    if (out_stat == (struct stat *)0) {
        errno = EINVAL;
        return -1;
    }
    (void)memset(out_stat, 0, sizeof(*out_stat));
    out_stat->st_mode = isatty(fd) ? 0UL : (S_IFREG | 0644UL);
    return 0;
}

__attribute__((weak)) int uname(struct utsname *out_name) {
    cleonos_sysinfo info;

    if (out_name == (struct utsname *)0) {
        errno = EINVAL;
        return -1;
    }

    (void)memset(&info, 0, sizeof(info));
    (void)cleonos_sys_sysinfo(&info);
    (void)memset(out_name, 0, sizeof(*out_name));
    posix_copy(out_name->sysname, sizeof(out_name->sysname), info.kernel_name[0] != '\0' ? info.kernel_name : "CLKS");
    posix_copy(out_name->nodename, sizeof(out_name->nodename), "cleonos");
    posix_copy(out_name->release, sizeof(out_name->release),
               info.kernel_version[0] != '\0' ? info.kernel_version : "unknown");
    posix_copy(out_name->version, sizeof(out_name->version), info.build_date);
    posix_copy(out_name->machine, sizeof(out_name->machine), info.arch[0] != '\0' ? info.arch : "x86_64");
    return 0;
}

__attribute__((weak)) DIR *opendir(const char *path) {
    DIR *dir;

    if (path == (const char *)0 || cleonos_sys_fs_stat_type(path) == 0ULL) {
        errno = ENOENT;
        return (DIR *)0;
    }

    dir = (DIR *)calloc(1U, sizeof(*dir));
    if (dir == (DIR *)0) {
        errno = ENOMEM;
        return (DIR *)0;
    }

    posix_copy(dir->path, sizeof(dir->path), path);
    dir->count = (unsigned long)cleonos_sys_fs_child_count(path);
    dir->index = 0UL;
    return dir;
}

__attribute__((weak)) struct dirent *readdir(DIR *dir) {
    char name[CLEONOS_FS_NAME_MAX];
    char full[256];
    u64 type;

    if (dir == (DIR *)0) {
        errno = EBADF;
        return (struct dirent *)0;
    }

    if (dir->index >= dir->count) {
        return (struct dirent *)0;
    }

    name[0] = '\0';
    if (cleonos_sys_fs_get_child_name(dir->path, (u64)dir->index, name) == 0ULL) {
        dir->index = dir->count;
        return (struct dirent *)0;
    }
    dir->index++;

    (void)memset(&dir->current, 0, sizeof(dir->current));
    posix_copy(dir->current.d_name, sizeof(dir->current.d_name), name);
    (void)snprintf(full, (unsigned long)sizeof(full), "%s%s%s", dir->path,
                   (strcmp(dir->path, "/") == 0) ? "" : "/", name);
    type = cleonos_sys_fs_stat_type(full);
    dir->current.d_type = (unsigned char)((type == 2ULL) ? DT_DIR : DT_REG);
    return &dir->current;
}

__attribute__((weak)) int closedir(DIR *dir) {
    if (dir == (DIR *)0) {
        errno = EBADF;
        return -1;
    }
    free(dir);
    return 0;
}
