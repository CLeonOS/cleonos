#include <cleonos_syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

int errno;

int open(const char *path, int flags, ...) {
    u64 fd;

    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    fd = cleonos_sys_fd_open(path, (u64)(unsigned int)flags, 0ULL);
    if (fd == (u64)-1) {
        errno = ENOENT;
        return -1;
    }

    return (int)fd;
}

int close(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    return (cleonos_sys_fd_close((u64)(unsigned int)fd) == (u64)-1) ? -1 : 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    u64 got;

    if (fd < 0 || (buf == (void *)0 && count != 0U)) {
        errno = EINVAL;
        return -1;
    }

    got = cleonos_sys_fd_read((u64)(unsigned int)fd, buf, (u64)count);
    if (got == (u64)-1) {
        errno = EIO;
        return -1;
    }

    return (ssize_t)got;
}

ssize_t write(int fd, const void *buf, size_t count) {
    u64 wrote;

    if (fd < 0 || (buf == (const void *)0 && count != 0U)) {
        errno = EINVAL;
        return -1;
    }

    wrote = cleonos_sys_fd_write((u64)(unsigned int)fd, buf, (u64)count);
    if (wrote == (u64)-1) {
        errno = EIO;
        return -1;
    }

    return (ssize_t)wrote;
}

off_t lseek(int fd, off_t offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return (off_t)-1;
}

int access(const char *path, int mode) {
    (void)mode;
    if (path == (const char *)0 || cleonos_sys_fs_stat_type(path) == (u64)-1) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int unlink(const char *path) {
    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }
    return (cleonos_sys_fs_remove(path) == (u64)-1) ? -1 : 0;
}
