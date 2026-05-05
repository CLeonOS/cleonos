#include "bdt_cleonos_compat.h"

#include <cleonos_syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dirent.h"

#define BDT_FILE_MAX 64
#define BDT_DIR_MAX 16
#define BDT_ENV_MAX 32
#define BDT_ENV_KEY_MAX 96
#define BDT_ENV_VALUE_MAX 512
#define BDT_CLEONOS_CMD_CTX_PATH "/temp/.ush_cmd_ctx.bin"
#define BDT_CLEONOS_CMD_MAX 32ULL
#define BDT_CLEONOS_ARG_MAX 160ULL
#define BDT_CLEONOS_PATH_MAX 192ULL

int errno;

struct bdt_cleonos_file {
    int fd;
    char *path;
    unsigned char *data;
    unsigned long size;
    unsigned long capacity;
    int readable;
    int writable;
    int dirty;
    int ungot;
    long pos;
};

struct bdt_cleonos_dir {
    char path[1024];
    u64 count;
    u64 index;
    struct dirent entry;
    int used;
};

struct bdt_cleonos_cmd_ctx {
    char cmd[BDT_CLEONOS_CMD_MAX];
    char arg[BDT_CLEONOS_ARG_MAX];
    char cwd[BDT_CLEONOS_PATH_MAX];
};

static struct bdt_cleonos_file bdt_stdin_obj = {0, (char *)0, (unsigned char *)0, 0, 0, 1, 0, 0, -1, 0};
static struct bdt_cleonos_file bdt_stdout_obj = {1, (char *)0, (unsigned char *)0, 0, 0, 0, 1, 0, -1, 0};
static struct bdt_cleonos_file bdt_stderr_obj = {2, (char *)0, (unsigned char *)0, 0, 0, 0, 1, 0, -1, 0};
static struct bdt_cleonos_file *bdt_fd_table[BDT_FILE_MAX];
static struct bdt_cleonos_dir bdt_dirs[BDT_DIR_MAX];
static char bdt_env_keys[BDT_ENV_MAX][BDT_ENV_KEY_MAX];
static char bdt_env_values[BDT_ENV_MAX][BDT_ENV_VALUE_MAX];

FILE *bdt_cleonos_stdin = &bdt_stdin_obj;
FILE *bdt_cleonos_stdout = &bdt_stdout_obj;
FILE *bdt_cleonos_stderr = &bdt_stderr_obj;

int *__errno_location(void) {
    return &errno;
}

char *strerror(int errnum) {
    switch (errnum) {
    case 0:
        return "no error";
    case ENOENT:
        return "not found";
    case EACCES:
        return "permission denied";
    case EBADF:
        return "bad file descriptor";
    case EINVAL:
        return "invalid argument";
    case ENOMEM:
        return "out of memory";
    case EIO:
        return "i/o error";
    case ENOSYS:
        return "not implemented";
    default:
        return "error";
    }
}

int bdt_cleonos_init(void) {
    unsigned long i;

    bdt_cleonos_stdin = &bdt_stdin_obj;
    bdt_cleonos_stdout = &bdt_stdout_obj;
    bdt_cleonos_stderr = &bdt_stderr_obj;
    bdt_fd_table[0] = bdt_cleonos_stdin;
    bdt_fd_table[1] = bdt_cleonos_stdout;
    bdt_fd_table[2] = bdt_cleonos_stderr;
    for (i = 3UL; i < BDT_FILE_MAX; i++) {
        bdt_fd_table[i] = (struct bdt_cleonos_file *)0;
    }
    memset(bdt_dirs, 0, sizeof(bdt_dirs));
    memset(bdt_env_keys, 0, sizeof(bdt_env_keys));
    memset(bdt_env_values, 0, sizeof(bdt_env_values));
    return 0;
}

void bdt_cleonos_import_env(char **envp) {
    unsigned int i;

    if (envp == (char **)0) {
        return;
    }

    for (i = 0U; envp[i] != (char *)0 && i < BDT_ENV_MAX; i++) {
        const char *entry = envp[i];
        const char *eq;
        char key[BDT_ENV_KEY_MAX];
        size_t key_len;

        if (entry == (const char *)0) {
            continue;
        }

        eq = strchr(entry, '=');
        if (eq == (const char *)0 || eq == entry) {
            continue;
        }

        key_len = (size_t)(eq - entry);
        if (key_len >= sizeof(key)) {
            key_len = sizeof(key) - 1U;
        }

        memcpy(key, entry, key_len);
        key[key_len] = '\0';
        (void)setenv(key, eq + 1, 1);
    }
}

static char *bdt_strdup_local(const char *text) {
    size_t len;
    char *out;

    if (text == (const char *)0) {
        return (char *)0;
    }

    len = strlen(text) + 1U;
    out = (char *)malloc(len);
    if (out != (char *)0) {
        memcpy(out, text, len);
    }
    return out;
}

static const char *bdt_cleonos_current_dir(void) {
    static char ctx_cwd[BDT_CLEONOS_PATH_MAX];
    const char *pwd = getenv("PWD");
    struct bdt_cleonos_cmd_ctx ctx;

    if (pwd != (const char *)0 && pwd[0] == '/') {
        return pwd;
    }

    memset(&ctx, 0, sizeof(ctx));
    if (cleonos_sys_fs_read(BDT_CLEONOS_CMD_CTX_PATH, (char *)&ctx, (u64)sizeof(ctx)) == (u64)sizeof(ctx) &&
        ctx.cwd[0] == '/') {
        snprintf(ctx_cwd, sizeof(ctx_cwd), "%s", ctx.cwd);
        return ctx_cwd;
    }

    return "/";
}

static int bdt_stream_reserve(struct bdt_cleonos_file *stream, unsigned long needed) {
    unsigned long cap;
    unsigned char *next;

    if (stream->capacity >= needed) {
        return 0;
    }

    cap = stream->capacity ? stream->capacity : 256UL;
    while (cap < needed) {
        cap *= 2UL;
    }

    next = (unsigned char *)realloc(stream->data, (size_t)cap);
    if (next == (unsigned char *)0) {
        errno = ENOMEM;
        return -1;
    }

    stream->data = next;
    stream->capacity = cap;
    return 0;
}

static int bdt_stream_flush_file(struct bdt_cleonos_file *stream) {
    if (stream == (struct bdt_cleonos_file *)0 || stream->path == (char *)0 || stream->dirty == 0) {
        return 0;
    }

    if (cleonos_sys_fs_write(stream->path, (const char *)stream->data, (u64)stream->size) == 0ULL) {
        errno = EIO;
        return -1;
    }

    stream->dirty = 0;
    return 0;
}

static struct bdt_cleonos_file *bdt_stream_for_fd(int fd) {
    if (fd < 0 || fd >= BDT_FILE_MAX || bdt_fd_table[fd] == (struct bdt_cleonos_file *)0) {
        errno = EBADF;
        return (struct bdt_cleonos_file *)0;
    }
    return bdt_fd_table[fd];
}

static int bdt_fd_alloc(struct bdt_cleonos_file *stream) {
    int i;

    for (i = 3; i < BDT_FILE_MAX; i++) {
        if (bdt_fd_table[i] == (struct bdt_cleonos_file *)0) {
            bdt_fd_table[i] = stream;
            stream->fd = i;
            return i;
        }
    }

    errno = EMFILE;
    return -1;
}

static int bdt_mode_to_flags(const char *mode, int *readable, int *writable) {
    int flags = O_RDONLY;
    int rd = 0;
    int wr = 0;

    if (mode == (const char *)0 || mode[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    if (mode[0] == 'r') {
        flags = O_RDONLY;
        rd = 1;
    } else if (mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        wr = 1;
    } else if (mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        wr = 1;
    } else {
        errno = EINVAL;
        return -1;
    }

    if (strchr(mode, '+') != (char *)0) {
        flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
        rd = 1;
        wr = 1;
    }

    *readable = rd;
    *writable = wr;
    return flags;
}

int open(const char *path, int flags, ...) {
    struct bdt_cleonos_file *stream;
    u64 size;
    int fd;

    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    stream = (struct bdt_cleonos_file *)calloc(1U, sizeof(*stream));
    if (stream == (struct bdt_cleonos_file *)0) {
        errno = ENOMEM;
        return -1;
    }

    stream->path = bdt_strdup_local(path);
    stream->readable = ((flags & O_WRONLY) == 0) || ((flags & O_RDWR) != 0);
    stream->writable = ((flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) != 0);
    stream->ungot = -1;

    size = cleonos_sys_fs_stat_size(path);
    if (size != (u64)-1 && (flags & O_TRUNC) == 0) {
        if (bdt_stream_reserve(stream, (unsigned long)size) != 0) {
            free(stream->path);
            free(stream);
            return -1;
        }
        if (size != 0ULL && cleonos_sys_fs_read(path, (char *)stream->data, size) != size) {
            free(stream->data);
            free(stream->path);
            free(stream);
            errno = EIO;
            return -1;
        }
        stream->size = (unsigned long)size;
    } else if ((flags & O_CREAT) == 0) {
        free(stream->path);
        free(stream);
        errno = ENOENT;
        return -1;
    }

    if ((flags & O_APPEND) != 0) {
        stream->pos = (long)stream->size;
    }

    fd = bdt_fd_alloc(stream);
    if (fd < 0) {
        free(stream->data);
        free(stream->path);
        free(stream);
        return -1;
    }

    return fd;
}

int close(int fd) {
    struct bdt_cleonos_file *stream = bdt_stream_for_fd(fd);

    if (stream == (struct bdt_cleonos_file *)0) {
        return -1;
    }

    if (bdt_stream_flush_file(stream) != 0) {
        return -1;
    }

    if (fd >= 3) {
        bdt_fd_table[fd] = (struct bdt_cleonos_file *)0;
        free(stream->data);
        free(stream->path);
        free(stream);
    }

    return 0;
}

ssize_t read(int fd, void *buf, unsigned long count) {
    struct bdt_cleonos_file *stream;
    unsigned long avail;

    if (fd < 0 || (buf == (void *)0 && count != 0UL)) {
        errno = EINVAL;
        return -1;
    }

    if (fd <= 2) {
        u64 got = cleonos_sys_fd_read((u64)fd, buf, (u64)count);
        if (got == (u64)-1) {
            errno = EIO;
            return -1;
        }
        return (ssize_t)got;
    }

    stream = bdt_stream_for_fd(fd);
    if (stream == (struct bdt_cleonos_file *)0) {
        return -1;
    }

    if (stream->pos < 0 || (unsigned long)stream->pos >= stream->size) {
        return 0;
    }

    avail = stream->size - (unsigned long)stream->pos;
    if (count > avail) {
        count = avail;
    }

    if (count != 0UL) {
        memcpy(buf, stream->data + stream->pos, (size_t)count);
        stream->pos += (long)count;
    }

    return (ssize_t)count;
}

ssize_t write(int fd, const void *buf, unsigned long count) {
    struct bdt_cleonos_file *stream;
    unsigned long end;

    if (fd < 0 || (buf == (const void *)0 && count != 0UL)) {
        errno = EINVAL;
        return -1;
    }

    if (fd <= 2) {
        u64 wrote = cleonos_sys_fd_write((u64)fd, buf, (u64)count);
        if (wrote == (u64)-1) {
            errno = EIO;
            return -1;
        }
        return (ssize_t)wrote;
    }

    stream = bdt_stream_for_fd(fd);
    if (stream == (struct bdt_cleonos_file *)0) {
        return -1;
    }

    if (stream->pos < 0) {
        errno = EINVAL;
        return -1;
    }

    end = (unsigned long)stream->pos + count;
    if (bdt_stream_reserve(stream, end) != 0) {
        return -1;
    }

    if (count != 0UL) {
        memcpy(stream->data + stream->pos, buf, (size_t)count);
        stream->pos += (long)count;
    }

    if (end > stream->size) {
        stream->size = end;
    }
    stream->dirty = 1;
    return (ssize_t)count;
}

off_t lseek(int fd, off_t offset, int whence) {
    struct bdt_cleonos_file *stream = bdt_stream_for_fd(fd);
    long base;
    long next;

    if (stream == (struct bdt_cleonos_file *)0) {
        return (off_t)-1;
    }

    if (fd <= 2) {
        errno = ESPIPE;
        return (off_t)-1;
    }

    if (whence == SEEK_SET) {
        base = 0L;
    } else if (whence == SEEK_CUR) {
        base = stream->pos;
    } else if (whence == SEEK_END) {
        base = (long)stream->size;
    } else {
        errno = EINVAL;
        return (off_t)-1;
    }

    next = base + (long)offset;
    if (next < 0L) {
        errno = EINVAL;
        return (off_t)-1;
    }

    stream->pos = next;
    stream->ungot = -1;
    return (off_t)next;
}

int unlink(const char *path) {
    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }
    return cleonos_sys_fs_remove(path) == 0ULL ? -1 : 0;
}

FILE *fopen(const char *path, const char *mode) {
    struct bdt_cleonos_file *stream;
    int readable;
    int writable;
    int flags;
    int fd;

    flags = bdt_mode_to_flags(mode, &readable, &writable);
    if (flags < 0) {
        return (FILE *)0;
    }

    fd = open(path, flags, 0);
    if (fd < 0) {
        return (FILE *)0;
    }

    stream = bdt_stream_for_fd(fd);
    if (stream == (struct bdt_cleonos_file *)0) {
        return (FILE *)0;
    }

    stream->readable = readable;
    stream->writable = writable;
    return stream;
}

int fclose(FILE *stream) {
    if (stream == (FILE *)0) {
        errno = EINVAL;
        return EOF;
    }
    return close(stream->fd) == 0 ? 0 : EOF;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t bytes;
    size_t done = 0;
    char *out = (char *)ptr;

    if (ptr == (void *)0 || stream == (FILE *)0 || size == 0U || nmemb == 0U) {
        return 0U;
    }

    bytes = size * nmemb;
    if (stream->ungot >= 0 && bytes > 0U) {
        out[done++] = (char)stream->ungot;
        stream->ungot = -1;
    }

    while (done < bytes) {
        ssize_t got = read(stream->fd, out + done, (unsigned long)(bytes - done));
        if (got <= 0) {
            break;
        }
        done += (size_t)got;
    }

    return done / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t bytes;
    size_t done = 0;
    const char *in = (const char *)ptr;

    if (ptr == (const void *)0 || stream == (FILE *)0 || size == 0U || nmemb == 0U) {
        return 0U;
    }

    bytes = size * nmemb;
    while (done < bytes) {
        ssize_t wrote = write(stream->fd, in + done, (unsigned long)(bytes - done));
        if (wrote <= 0) {
            break;
        }
        done += (size_t)wrote;
    }

    return done / size;
}

int fgetc(FILE *stream) {
    unsigned char ch;

    if (stream == (FILE *)0) {
        errno = EINVAL;
        return EOF;
    }

    if (stream->ungot >= 0) {
        int ret = stream->ungot;
        stream->ungot = -1;
        return ret;
    }

    if (read(stream->fd, &ch, 1UL) != 1) {
        return EOF;
    }

    return (int)ch;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

int ungetc(int ch, FILE *stream) {
    if (stream == (FILE *)0 || ch == EOF || stream->ungot >= 0) {
        return EOF;
    }
    stream->ungot = ch & 0xFF;
    return stream->ungot;
}

char *fgets(char *out, int size, FILE *stream) {
    int pos = 0;

    if (out == (char *)0 || size <= 0 || stream == (FILE *)0) {
        errno = EINVAL;
        return (char *)0;
    }

    while (pos + 1 < size) {
        int ch = fgetc(stream);
        if (ch == EOF) {
            break;
        }
        out[pos++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (pos == 0) {
        return (char *)0;
    }

    out[pos] = '\0';
    return out;
}

int fputc(int ch, FILE *stream) {
    unsigned char c = (unsigned char)ch;

    if (stream == (FILE *)0 || write(stream->fd, &c, 1UL) != 1) {
        return EOF;
    }

    return (int)c;
}

int putchar(int ch) {
    return fputc(ch, stdout);
}

int getchar(void) {
    return fgetc(stdin);
}

int fputs(const char *text, FILE *stream) {
    size_t len;

    if (text == (const char *)0 || stream == (FILE *)0) {
        errno = EINVAL;
        return EOF;
    }

    len = strlen(text);
    return fwrite(text, 1U, len, stream) == len ? 0 : EOF;
}

int puts(const char *text) {
    if (fputs(text, stdout) == EOF) {
        return EOF;
    }
    return fputc('\n', stdout);
}

int fflush(FILE *stream) {
    if (stream == (FILE *)0) {
        return 0;
    }
    return bdt_stream_flush_file(stream);
}

int vfprintf(FILE *stream, const char *fmt, va_list args) {
    char buf[2048];
    int n;

    if (stream == (FILE *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    n = vsnprintf(buf, (unsigned long)sizeof(buf), fmt, args);
    if (n < 0) {
        return EOF;
    }

    if (fwrite(buf, 1U, (size_t)n, stream) != (size_t)n) {
        return EOF;
    }

    return n;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vfprintf(stream, fmt, args);
    va_end(args);
    return ret;
}

int vprintf(const char *fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
}

int printf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}

static int bdt_scan_u64(const char *text, unsigned long long *out_value) {
    unsigned long long value = 0ULL;
    const char *p = text;
    int any = 0;

    while (*p >= '0' && *p <= '9') {
        value = (value * 10ULL) + (unsigned long long)(*p - '0');
        any = 1;
        p++;
    }

    if (any == 0 || out_value == (unsigned long long *)0) {
        return 0;
    }

    *out_value = value;
    return 1;
}

int sscanf(const char *text, const char *fmt, ...) {
    va_list args;
    int matched = 0;

    if (text == (const char *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    va_start(args, fmt);
    if (strcmp(fmt, "%llu") == 0 || strcmp(fmt, "%lu") == 0) {
        unsigned long long value = 0ULL;
        if (bdt_scan_u64(text, &value) != 0) {
            unsigned long long *out = va_arg(args, unsigned long long *);
            *out = value;
            matched = 1;
        }
    } else if (strcmp(fmt, "%95[^=]=%llu") == 0 || strcmp(fmt, "%95[^=]=%lu") == 0) {
        char *key = va_arg(args, char *);
        unsigned long long *out = va_arg(args, unsigned long long *);
        size_t i = 0U;
        while (text[i] != '\0' && text[i] != '=' && i < 95U) {
            key[i] = text[i];
            i++;
        }
        key[i] = '\0';
        if (text[i] == '=' && bdt_scan_u64(text + i + 1U, out) != 0) {
            matched = 2;
        }
    }
    va_end(args);

    return matched;
}

int fscanf(FILE *stream, const char *fmt, ...) {
    char line[256];
    va_list args;
    int matched = 0;

    if (stream == (FILE *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    if (fgets(line, (int)sizeof(line), stream) == (char *)0) {
        return EOF;
    }

    va_start(args, fmt);
    if (strcmp(fmt, "%llu") == 0 || strcmp(fmt, "%lu") == 0) {
        unsigned long long value = 0ULL;
        if (bdt_scan_u64(line, &value) != 0) {
            unsigned long long *out = va_arg(args, unsigned long long *);
            *out = value;
            matched = 1;
        }
    }
    va_end(args);

    return matched;
}

int remove(const char *path) {
    return unlink(path);
}

int stat(const char *path, struct stat *out) {
    u64 type;
    u64 size;

    if (path == (const char *)0 || out == (struct stat *)0) {
        errno = EINVAL;
        return -1;
    }

    type = cleonos_sys_fs_stat_type(path);
    if (type == (u64)-1) {
        errno = ENOENT;
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->st_mode = (type == 2ULL) ? S_IFDIR : S_IFREG;
    size = cleonos_sys_fs_stat_size(path);
    out->st_size = (size == (u64)-1) ? 0UL : (unsigned long)size;
    out->st_mtime = (long)(cleonos_sys_time_ms() / 1000ULL);
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    (void)mode;
    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }
    return cleonos_sys_fs_mkdir(path) != 0ULL ? 0 : -1;
}

DIR *opendir(const char *path) {
    int i;

    if (path == (const char *)0 || cleonos_sys_fs_stat_type(path) != 2ULL) {
        errno = ENOTDIR;
        return (DIR *)0;
    }

    for (i = 0; i < BDT_DIR_MAX; i++) {
        if (bdt_dirs[i].used == 0) {
            memset(&bdt_dirs[i], 0, sizeof(bdt_dirs[i]));
            bdt_dirs[i].used = 1;
            snprintf(bdt_dirs[i].path, sizeof(bdt_dirs[i].path), "%s", path);
            bdt_dirs[i].count = cleonos_sys_fs_child_count(path);
            return (DIR *)&bdt_dirs[i];
        }
    }

    errno = EMFILE;
    return (DIR *)0;
}

struct dirent *readdir(DIR *dir) {
    struct bdt_cleonos_dir *d = (struct bdt_cleonos_dir *)dir;
    char child[1200];
    u64 type;

    if (d == (struct bdt_cleonos_dir *)0 || d->used == 0) {
        errno = EBADF;
        return (struct dirent *)0;
    }

    while (d->index < d->count) {
        d->entry.d_name[0] = '\0';
        if (cleonos_sys_fs_get_child_name(d->path, d->index++, d->entry.d_name) == 0ULL) {
            continue;
        }

        snprintf(child, sizeof(child), "%s%s%s", d->path, (d->path[strlen(d->path) - 1U] == '/') ? "" : "/",
                 d->entry.d_name);
        type = cleonos_sys_fs_stat_type(child);
        d->entry.d_type = (type == 2ULL) ? DT_DIR : ((type == 1ULL) ? DT_REG : DT_UNKNOWN);
        return &d->entry;
    }

    return (struct dirent *)0;
}

int closedir(DIR *dir) {
    struct bdt_cleonos_dir *d = (struct bdt_cleonos_dir *)dir;

    if (d == (struct bdt_cleonos_dir *)0 || d->used == 0) {
        errno = EBADF;
        return -1;
    }

    memset(d, 0, sizeof(*d));
    return 0;
}

time_t time(time_t *out_time) {
    time_t now = (time_t)(cleonos_sys_time_ms() / 1000ULL);
    if (out_time != (time_t *)0) {
        *out_time = now;
    }
    return now;
}

clock_t clock(void) {
    return (clock_t)cleonos_sys_time_ms();
}

struct tm *localtime(const time_t *timer) {
    static struct tm tmv;
    time_t t = timer ? *timer : time((time_t *)0);
    unsigned long days = (unsigned long)(t / 86400L);
    unsigned long secs = (unsigned long)(t % 86400L);

    tmv.tm_hour = (int)(secs / 3600UL);
    tmv.tm_min = (int)((secs / 60UL) % 60UL);
    tmv.tm_sec = (int)(secs % 60UL);
    tmv.tm_mday = (int)((days % 30UL) + 1UL);
    tmv.tm_mon = (int)((days / 30UL) % 12UL);
    tmv.tm_year = 126;
    tmv.tm_wday = (int)((days + 4UL) % 7UL);
    tmv.tm_yday = (int)(days % 365UL);
    tmv.tm_isdst = 0;
    return &tmv;
}

char *getenv(const char *name) {
    unsigned int i;

    if (name == (const char *)0 || name[0] == '\0') {
        return (char *)0;
    }

    for (i = 0U; i < BDT_ENV_MAX; i++) {
        if (bdt_env_keys[i][0] != '\0' && strcmp(bdt_env_keys[i], name) == 0) {
            return bdt_env_values[i];
        }
    }

    return (char *)0;
}

int setenv(const char *name, const char *value, int overwrite) {
    unsigned int i;
    int empty = -1;

    if (name == (const char *)0 || name[0] == '\0' || strchr(name, '=') != (char *)0) {
        errno = EINVAL;
        return -1;
    }

    for (i = 0U; i < BDT_ENV_MAX; i++) {
        if (bdt_env_keys[i][0] == '\0') {
            if (empty < 0) {
                empty = (int)i;
            }
        } else if (strcmp(bdt_env_keys[i], name) == 0) {
            if (overwrite == 0) {
                return 0;
            }
            snprintf(bdt_env_values[i], sizeof(bdt_env_values[i]), "%s", value ? value : "");
            return 0;
        }
    }

    if (empty < 0) {
        errno = ENOMEM;
        return -1;
    }

    snprintf(bdt_env_keys[empty], sizeof(bdt_env_keys[empty]), "%s", name);
    snprintf(bdt_env_values[empty], sizeof(bdt_env_values[empty]), "%s", value ? value : "");
    return 0;
}

static int bdt_system_split_command(const char *cmd, char *out_path, size_t out_path_size, char *out_args,
                                    size_t out_args_size) {
    size_t i = 0U;
    size_t p = 0U;

    if (cmd == (const char *)0 || out_path == (char *)0 || out_args == (char *)0 || out_path_size == 0U ||
        out_args_size == 0U) {
        errno = EINVAL;
        return -1;
    }

    while (cmd[i] == ' ' || cmd[i] == '\t') {
        i++;
    }

    if (cmd[i] == '\0') {
        errno = EINVAL;
        return -1;
    }

    if (cmd[i] == '"' || cmd[i] == '\'') {
        char quote = cmd[i++];
        while (cmd[i] != '\0' && cmd[i] != quote && p + 1U < out_path_size) {
            out_path[p++] = cmd[i++];
        }
        if (cmd[i] == quote) {
            i++;
        }
    } else {
        while (cmd[i] != '\0' && cmd[i] != ' ' && cmd[i] != '\t' && p + 1U < out_path_size) {
            out_path[p++] = cmd[i++];
        }
    }

    out_path[p] = '\0';
    while (cmd[i] == ' ' || cmd[i] == '\t') {
        i++;
    }
    snprintf(out_args, out_args_size, "%s", cmd + i);
    return (out_path[0] != '\0') ? 0 : -1;
}

int system(const char *cmd) {
    char path[160];
    char args[256];
    u64 status;

    if (bdt_system_split_command(cmd, path, sizeof(path), args, sizeof(args)) != 0) {
        return -1;
    }

    if (path[0] != '/') {
        errno = ENOSYS;
        return -1;
    }

    status = cleonos_sys_exec_pathv(path, args, "PWD=/");
    if (status == (u64)-1) {
        errno = EIO;
        return -1;
    }

    return status == 0ULL ? 0 : (int)status;
}

char *realpath(const char *path, char *resolved_path) {
    char *out = resolved_path;
    const char *cwd;
    size_t len;

    if (path == (const char *)0) {
        errno = EINVAL;
        return (char *)0;
    }

    cwd = bdt_cleonos_current_dir();

    if (strcmp(path, ".") == 0 || strcmp(path, "/.") == 0) {
        len = strlen(cwd);
    } else if (path[0] == '/') {
        len = strlen(path);
    } else {
        len = strlen(cwd) + 1U + strlen(path);
    }
    if (out == (char *)0) {
        out = (char *)malloc(len + 2U);
        if (out == (char *)0) {
            errno = ENOMEM;
            return (char *)0;
        }
    }

    if (strcmp(path, ".") == 0 || strcmp(path, "/.") == 0) {
        strcpy(out, cwd);
    } else if (path[0] == '/') {
        strcpy(out, path);
    } else {
        snprintf(out, len + 2U, "%s%s%s", cwd, (cwd[strlen(cwd) - 1U] == '/') ? "" : "/", path);
    }

    return out;
}

char *getcwd(char *buf, unsigned long size) {
    const char *cwd;
    size_t len;

    if (buf == (char *)0 || size < 2UL) {
        errno = EINVAL;
        return (char *)0;
    }

    cwd = bdt_cleonos_current_dir();

    len = strlen(cwd);
    if (len + 1U > (size_t)size) {
        errno = ERANGE;
        return (char *)0;
    }

    strcpy(buf, cwd);
    return buf;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *items = (unsigned char *)base;
    int changed = 1;

    if (base == (void *)0 || compar == (int (*)(const void *, const void *))0 || size == 0U) {
        return;
    }

    while (changed != 0) {
        size_t i;
        changed = 0;
        for (i = 1U; i < nmemb; i++) {
            unsigned char *left = items + ((i - 1U) * size);
            unsigned char *right = items + (i * size);
            if (compar(left, right) > 0) {
                size_t j;
                for (j = 0U; j < size; j++) {
                    unsigned char tmp = left[j];
                    left[j] = right[j];
                    right[j] = tmp;
                }
                changed = 1;
            }
        }
    }
}

int ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    (void)request;
    errno = ENOSYS;
    return -1;
}

int mkstemp(char *template) {
    (void)template;
    errno = ENOSYS;
    return -1;
}
