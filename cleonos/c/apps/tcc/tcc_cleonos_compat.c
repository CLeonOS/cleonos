#include "tcc_cleonos_compat.h"

#include <cleonos_syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

int errno;

struct cleonos_tcc_file {
    int fd;
    char *path;
    unsigned char *data;
    unsigned long size;
    unsigned long capacity;
    int readable;
    int writable;
    int close_on_fclose;
    int dirty;
    int ungot;
    long pos;
};

#define CLEONOS_TCC_MAX_OPEN_FILES 64

static struct cleonos_tcc_file cleonos_tcc_stdin_obj = {0, (char *)0, (unsigned char *)0, 0, 0, 1, 0, 0, 0, -1, 0};
static struct cleonos_tcc_file cleonos_tcc_stdout_obj = {1, (char *)0, (unsigned char *)0, 0, 0, 0, 1, 0, 0, -1, 0};
static struct cleonos_tcc_file cleonos_tcc_stderr_obj = {2, (char *)0, (unsigned char *)0, 0, 0, 0, 1, 0, 0, -1, 0};
static struct cleonos_tcc_file *cleonos_tcc_fd_table[CLEONOS_TCC_MAX_OPEN_FILES];

FILE *stdin = &cleonos_tcc_stdin_obj;
FILE *stdout = &cleonos_tcc_stdout_obj;
FILE *stderr = &cleonos_tcc_stderr_obj;

int cleonos_tcc_stdio_init(void) {
    unsigned long i;

    stdin = &cleonos_tcc_stdin_obj;
    stdout = &cleonos_tcc_stdout_obj;
    stderr = &cleonos_tcc_stderr_obj;
    cleonos_tcc_fd_table[0] = stdin;
    cleonos_tcc_fd_table[1] = stdout;
    cleonos_tcc_fd_table[2] = stderr;
    for (i = 3UL; i < CLEONOS_TCC_MAX_OPEN_FILES; i++) {
        cleonos_tcc_fd_table[i] = (struct cleonos_tcc_file *)0;
    }
    return 0;
}

static char *tcc_strdup_local(const char *text) {
    size_t len;
    char *out;

    if (text == (const char *)0) {
        return (char *)0;
    }

    len = strlen(text) + 1U;
    out = (char *)malloc(len);
    if (out != (char *)0) {
        (void)memcpy(out, text, len);
    }
    return out;
}

static int tcc_stream_reserve(struct cleonos_tcc_file *stream, unsigned long needed) {
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

static int tcc_stream_flush_file(struct cleonos_tcc_file *stream) {
    if (stream == (struct cleonos_tcc_file *)0 || stream->path == (char *)0 || stream->dirty == 0) {
        return 0;
    }

    if (cleonos_sys_fs_write(stream->path, (const char *)stream->data, (u64)stream->size) == (u64)-1) {
        errno = EIO;
        return -1;
    }

    stream->dirty = 0;
    return 0;
}

static struct cleonos_tcc_file *tcc_stream_for_fd(int fd) {
    if (fd < 0 || fd >= CLEONOS_TCC_MAX_OPEN_FILES) {
        errno = EBADF;
        return (struct cleonos_tcc_file *)0;
    }
    if (cleonos_tcc_fd_table[fd] == (struct cleonos_tcc_file *)0) {
        errno = EBADF;
        return (struct cleonos_tcc_file *)0;
    }
    return cleonos_tcc_fd_table[fd];
}

static int tcc_fd_alloc(struct cleonos_tcc_file *stream) {
    int i;

    for (i = 3; i < CLEONOS_TCC_MAX_OPEN_FILES; i++) {
        if (cleonos_tcc_fd_table[i] == (struct cleonos_tcc_file *)0) {
            cleonos_tcc_fd_table[i] = stream;
            stream->fd = i;
            return i;
        }
    }

    errno = EMFILE;
    return -1;
}

static int tcc_mode_to_flags(const char *mode, int *readable, int *writable) {
    int flags = O_RDONLY;
    int rd = 0;
    int wr = 0;

    if (mode == (const char *)0 || mode[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    switch (mode[0]) {
    case 'r':
        flags = O_RDONLY;
        rd = 1;
        break;
    case 'w':
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        wr = 1;
        break;
    case 'a':
        flags = O_WRONLY | O_CREAT | O_APPEND;
        wr = 1;
        break;
    default:
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
    struct cleonos_tcc_file *stream;
    u64 size;
    int fd;

    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    stream = (struct cleonos_tcc_file *)calloc(1U, sizeof(*stream));
    if (stream == (struct cleonos_tcc_file *)0) {
        errno = ENOMEM;
        return -1;
    }

    stream->path = tcc_strdup_local(path);
    stream->readable = ((flags & O_WRONLY) == 0) || ((flags & O_RDWR) != 0);
    stream->writable = ((flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND)) != 0);
    stream->close_on_fclose = 1;
    stream->ungot = -1;

    size = cleonos_sys_fs_stat_size(path);
    if (size != (u64)-1 && (flags & O_TRUNC) == 0) {
        if (tcc_stream_reserve(stream, (unsigned long)size) != 0) {
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

    fd = tcc_fd_alloc(stream);
    if (fd < 0) {
        free(stream->data);
        free(stream->path);
        free(stream);
        return -1;
    }

    return fd;
}

int close(int fd) {
    struct cleonos_tcc_file *stream = tcc_stream_for_fd(fd);

    if (stream == (struct cleonos_tcc_file *)0) {
        return -1;
    }

    if (tcc_stream_flush_file(stream) != 0) {
        return -1;
    }

    if (fd >= 3) {
        cleonos_tcc_fd_table[fd] = (struct cleonos_tcc_file *)0;
        free(stream->data);
        free(stream->path);
        free(stream);
    }

    return 0;
}

ssize_t read(int fd, void *buf, unsigned long count) {
    struct cleonos_tcc_file *stream;
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

    stream = tcc_stream_for_fd(fd);
    if (stream == (struct cleonos_tcc_file *)0) {
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
        (void)memcpy(buf, stream->data + stream->pos, (size_t)count);
        stream->pos += (long)count;
    }

    return (ssize_t)count;
}

ssize_t write(int fd, const void *buf, unsigned long count) {
    struct cleonos_tcc_file *stream;
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

    stream = tcc_stream_for_fd(fd);
    if (stream == (struct cleonos_tcc_file *)0) {
        return -1;
    }

    if (stream->pos < 0) {
        errno = EINVAL;
        return -1;
    }

    end = (unsigned long)stream->pos + count;
    if (tcc_stream_reserve(stream, end) != 0) {
        return -1;
    }

    if (count != 0UL) {
        (void)memcpy(stream->data + stream->pos, buf, (size_t)count);
        stream->pos += (long)count;
    }

    if (end > stream->size) {
        stream->size = end;
    }
    stream->dirty = 1;
    return (ssize_t)count;
}

off_t lseek(int fd, off_t offset, int whence) {
    struct cleonos_tcc_file *stream = tcc_stream_for_fd(fd);
    long base;
    long next;

    if (stream == (struct cleonos_tcc_file *)0) {
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

int access(const char *path, int mode) {
    (void)mode;
    if (path == (const char *)0 || cleonos_sys_fs_stat_type(path) == (u64)-1) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

char *getcwd(char *buf, unsigned long size) {
    if (buf == (char *)0 || size == 0UL) {
        errno = EINVAL;
        return (char *)0;
    }
    if (size < 2UL) {
        errno = ERANGE;
        return (char *)0;
    }
    buf[0] = '/';
    buf[1] = '\0';
    return buf;
}

int unlink(const char *path) {
    if (path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }
    return cleonos_sys_fs_remove(path) == (u64)-1 ? -1 : 0;
}

int execvp(const char *file, char *const argv[]) {
    (void)file;
    (void)argv;
    errno = ENOSYS;
    return -1;
}

FILE *fdopen(int fd, const char *mode) {
    struct cleonos_tcc_file *stream = tcc_stream_for_fd(fd);
    (void)mode;

    if (stream == (struct cleonos_tcc_file *)0) {
        errno = EINVAL;
        return (FILE *)0;
    }

    return stream;
}

FILE *fopen(const char *path, const char *mode) {
    struct cleonos_tcc_file *stream;
    int readable;
    int writable;
    int flags;
    int fd;

    flags = tcc_mode_to_flags(mode, &readable, &writable);
    if (flags < 0) {
        return (FILE *)0;
    }

    fd = open(path, flags, 0);
    if (fd < 0) {
        return (FILE *)0;
    }

    stream = tcc_stream_for_fd(fd);
    if (stream == (struct cleonos_tcc_file *)0) {
        return (FILE *)0;
    }
    stream->readable = readable;
    stream->writable = writable;
    return stream;
}

int fclose(FILE *stream) {
    int ret = 0;

    if (stream == (FILE *)0) {
        errno = EINVAL;
        return EOF;
    }

    ret = close(stream->fd);
    return ret == 0 ? 0 : EOF;
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

int fseek(FILE *stream, long offset, int whence) {
    off_t pos;

    if (stream == (FILE *)0) {
        errno = EINVAL;
        return -1;
    }

    pos = lseek(stream->fd, (off_t)offset, whence);
    if (pos == (off_t)-1) {
        return -1;
    }

    stream->pos = (long)pos;
    stream->ungot = -1;
    return 0;
}

long ftell(FILE *stream) {
    if (stream == (FILE *)0) {
        errno = EINVAL;
        return -1L;
    }
    return stream->pos;
}

int fileno(FILE *stream) {
    if (stream == (FILE *)0) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
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
    return tcc_stream_flush_file(stream);
}

int vfprintf(FILE *stream, const char *fmt, va_list args) {
    char buf[2048];
    int n;

    if (stream == (FILE *)0 || fmt == (const char *)0) {
        errno = EINVAL;
        return EOF;
    }

    n = vsnprintf(buf, sizeof(buf), fmt, args);
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

int sprintf(char *out, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(out, (unsigned long)-1, fmt, args);
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

int remove(const char *path) {
    return unlink(path);
}

int rename(const char *old_path, const char *new_path) {
    u64 size;
    char *buf;
    u64 got;

    if (old_path == (const char *)0 || new_path == (const char *)0) {
        errno = EINVAL;
        return -1;
    }

    size = cleonos_sys_fs_stat_size(old_path);
    if (size == (u64)-1) {
        errno = ENOENT;
        return -1;
    }

    buf = (char *)malloc((size_t)size);
    if (buf == (char *)0 && size != 0ULL) {
        errno = ENOMEM;
        return -1;
    }

    got = cleonos_sys_fs_read(old_path, buf, size);
    if (got == (u64)-1 || got != size || cleonos_sys_fs_write(new_path, buf, size) == (u64)-1) {
        free(buf);
        errno = EIO;
        return -1;
    }

    free(buf);
    return unlink(old_path);
}

char *realpath(const char *path, char *resolved_path) {
    char *out = resolved_path;
    size_t len;

    if (path == (const char *)0) {
        errno = EINVAL;
        return (char *)0;
    }

    len = strlen(path);
    if (out == (char *)0) {
        out = (char *)malloc(len + 2U);
        if (out == (char *)0) {
            errno = ENOMEM;
            return (char *)0;
        }
    }

    if (path[0] == '/') {
        (void)strcpy(out, path);
    } else {
        out[0] = '/';
        (void)strcpy(out + 1, path);
    }

    return out;
}

int gettimeofday(struct timeval *tv, void *tz) {
    u64 ms;

    (void)tz;
    if (tv == (struct timeval *)0) {
        errno = EINVAL;
        return -1;
    }

    ms = cleonos_sys_time_ms();
    tv->tv_sec = (long)(ms / 1000ULL);
    tv->tv_usec = (long)((ms % 1000ULL) * 1000ULL);
    return 0;
}

char *getenv(const char *name) {
    (void)name;
    return (char *)0;
}

const char *strerror(int errnum) {
    switch (errnum) {
    case ENOENT:
        return "not found";
    case ENOMEM:
        return "out of memory";
    case EINVAL:
        return "invalid argument";
    case EIO:
        return "I/O error";
    case EBADF:
        return "bad file descriptor";
    case ENOSYS:
        return "not implemented";
    default:
        return "error";
    }
}

double atof(const char *text) {
    return strtod(text, (char **)0);
}

float strtof(const char *text, char **out_end) {
    return (float)strtod(text, out_end);
}

long double strtold(const char *text, char **out_end) {
    return (long double)strtod(text, out_end);
}

long double ldexpl(long double value, int exp) {
    long double scale = 1.0L;

    while (exp > 0) {
        scale *= 2.0L;
        exp--;
    }
    while (exp < 0) {
        scale *= 0.5L;
        exp++;
    }
    return value * scale;
}

static void tcc_qsort_swap(unsigned char *a, unsigned char *b, size_t size) {
    while (size-- != 0U) {
        unsigned char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
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
                tcc_qsort_swap(left, right, size);
                changed = 1;
            }
        }
    }
}

static struct tm cleonos_tcc_tm;

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
    time_t t = timer ? *timer : time((time_t *)0);
    unsigned long days = (unsigned long)(t / 86400L);
    unsigned long secs = (unsigned long)(t % 86400L);

    cleonos_tcc_tm.tm_hour = (int)(secs / 3600UL);
    cleonos_tcc_tm.tm_min = (int)((secs / 60UL) % 60UL);
    cleonos_tcc_tm.tm_sec = (int)(secs % 60UL);
    cleonos_tcc_tm.tm_mday = (int)((days % 30UL) + 1UL);
    cleonos_tcc_tm.tm_mon = (int)((days / 30UL) % 12UL);
    cleonos_tcc_tm.tm_year = 126;
    cleonos_tcc_tm.tm_wday = (int)((days + 4UL) % 7UL);
    cleonos_tcc_tm.tm_yday = (int)(days % 365UL);
    cleonos_tcc_tm.tm_isdst = 0;
    return &cleonos_tcc_tm;
}
