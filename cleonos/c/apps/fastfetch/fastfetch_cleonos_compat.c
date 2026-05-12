#include "fastfetch_cleonos_compat.h"

#include <cleonos_syscall.h>

#undef fputs
#undef fputc
#undef puts
#undef putchar
#undef printf
#undef vprintf
#undef fprintf
#undef vfprintf
#undef sprintf
#undef vasprintf
#undef strcasestr
#undef qsort
#undef system
#undef fwrite
#undef fflush
#undef setvbuf
#undef fopen
#undef fclose
#undef fread
#undef fseek
#undef ftell
#undef feof
#undef fileno
#undef getenv
#undef setlocale
#undef atexit
#undef exit
#undef access
#undef mkdir
#undef realpath
#undef readlink
#undef openat
#undef dup2
#undef tcgetattr
#undef tcsetattr
#undef gethostname
#undef clock_gettime
#undef nanosleep
#undef mbrtowc

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE g_stdin_file = {0};
static FILE g_stdout_file = {1};
static FILE g_stderr_file = {2};

FILE *stdin = &g_stdin_file;
FILE *stdout = &g_stdout_file;
FILE *stderr = &g_stderr_file;
jmp_buf ff_cl_exit_jmp;
int ff_cl_exit_status = 0;

static int ff_cl_fd(FILE *stream) {
    return stream != (FILE *)0 ? stream->fd : 1;
}

int ff_cl_fputs(const char *text, FILE *stream) {
    return fputs(text, ff_cl_fd(stream));
}

int ff_cl_fputc(int ch, FILE *stream) {
    char c = (char)ch;
    return cleonos_sys_fd_write((u64)(unsigned int)ff_cl_fd(stream), &c, 1ULL) == 1ULL ? ch : EOF;
}

int ff_cl_puts(const char *text) {
    int rc = fputs(text, 1);
    if (rc == EOF) {
        return EOF;
    }
    return fputc('\n', 1);
}

int ff_cl_putchar(int ch) {
    return fputc(ch, 1);
}

int ff_cl_vprintf(const char *fmt, va_list args) {
    return vdprintf(1, fmt, args);
}

int ff_cl_printf(const char *fmt, ...) {
    va_list args;
    int rc;
    va_start(args, fmt);
    rc = vdprintf(1, fmt, args);
    va_end(args);
    return rc;
}

int ff_cl_vfprintf(FILE *stream, const char *fmt, va_list args) {
    return vdprintf(ff_cl_fd(stream), fmt, args);
}

int ff_cl_fprintf(FILE *stream, const char *fmt, ...) {
    va_list args;
    int rc;
    va_start(args, fmt);
    rc = vdprintf(ff_cl_fd(stream), fmt, args);
    va_end(args);
    return rc;
}

int ff_cl_sprintf(char *out, const char *fmt, ...) {
    va_list args;
    int rc;
    va_start(args, fmt);
    rc = vsnprintf(out, (unsigned long)-1, fmt, args);
    va_end(args);
    return rc;
}

int ff_cl_vasprintf(char **out, const char *fmt, va_list args) {
    va_list copy;
    int len;
    char *buf;
    if (out == (char **)0) {
        return -1;
    }
    va_copy(copy, args);
    len = vsnprintf((char *)0, 0UL, fmt, copy);
    va_end(copy);
    if (len < 0) {
        *out = (char *)0;
        return -1;
    }
    buf = (char *)malloc((size_t)len + 1U);
    if (buf == (char *)0) {
        *out = (char *)0;
        return -1;
    }
    (void)vsnprintf(buf, (unsigned long)((size_t)len + 1U), fmt, args);
    *out = buf;
    return len;
}

char *ff_cl_strcasestr(const char *haystack, const char *needle) {
    size_t needle_len;
    if (haystack == (const char *)0 || needle == (const char *)0) {
        return (char *)0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0U) {
        return (char *)haystack;
    }
    while (*haystack != '\0') {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return (char *)0;
}

static void ff_cl_qsort_swap(unsigned char *a, unsigned char *b, size_t size) {
    while (size-- > 0U) {
        unsigned char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void ff_cl_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *data = (unsigned char *)base;
    size_t i;
    int swapped;
    if (data == (unsigned char *)0 || compar == (int (*)(const void *, const void *))0 || size == 0U) {
        return;
    }
    do {
        swapped = 0;
        for (i = 1U; i < nmemb; i++) {
            unsigned char *left = data + (i - 1U) * size;
            unsigned char *right = data + i * size;
            if (compar(left, right) > 0) {
                ff_cl_qsort_swap(left, right, size);
                swapped = 1;
            }
        }
    } while (swapped != 0);
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    ff_cl_qsort(base, nmemb, size, compar);
}

int ff_cl_system(const char *command) {
    (void)command;
    return -1;
}

int system(const char *command) {
    return ff_cl_system(command);
}

double round(double value) {
    long long whole = (long long)value;
    double frac = value - (double)whole;
    if (value >= 0.0) {
        return frac >= 0.5 ? (double)(whole + 1LL) : (double)whole;
    }
    return frac <= -0.5 ? (double)(whole - 1LL) : (double)whole;
}

size_t ff_cl_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    u64 bytes = (u64)(size * nmemb);
    u64 wrote;

    if (bytes == 0ULL) {
        return 0U;
    }
    wrote = cleonos_sys_fd_write((u64)(unsigned int)ff_cl_fd(stream), ptr, bytes);
    if (wrote == (u64)-1) {
        return 0U;
    }
    return (size == 0U) ? 0U : (size_t)(wrote / (u64)size);
}

int ff_cl_fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int ff_cl_setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    (void)stream;
    (void)buf;
    (void)mode;
    (void)size;
    return 0;
}

FILE *ff_cl_fopen(const char *path, const char *mode) {
    FILE *file;
    int flags = O_RDONLY;
    int fd;

    if (mode != (const char *)0 && (strchr(mode, 'w') != (char *)0 || strchr(mode, 'a') != (char *)0)) {
        flags = O_WRONLY | O_CREAT;
        if (strchr(mode, 'a') != (char *)0) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
    }
    fd = open(path, flags, 0644);
    if (fd < 0) {
        return (FILE *)0;
    }
    file = (FILE *)malloc(sizeof(*file));
    if (file == (FILE *)0) {
        (void)close(fd);
        return (FILE *)0;
    }
    file->fd = fd;
    return file;
}

int ff_cl_fclose(FILE *stream) {
    int fd;
    if (stream == (FILE *)0 || stream == stdin || stream == stdout || stream == stderr) {
        return 0;
    }
    fd = stream->fd;
    free(stream);
    return close(fd);
}

size_t ff_cl_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    u64 bytes = (u64)(size * nmemb);
    u64 got;
    if (bytes == 0ULL) {
        return 0U;
    }
    got = cleonos_sys_fd_read((u64)(unsigned int)ff_cl_fd(stream), ptr, bytes);
    if (got == (u64)-1) {
        return 0U;
    }
    return (size == 0U) ? 0U : (size_t)(got / (u64)size);
}

int ff_cl_fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return -1;
}

long ff_cl_ftell(FILE *stream) {
    (void)stream;
    return -1L;
}

int ff_cl_feof(FILE *stream) {
    (void)stream;
    return 0;
}

int ff_cl_fileno(FILE *stream) {
    return ff_cl_fd(stream);
}

char *ff_cl_getenv(const char *name) {
    static char value[192];
    u64 envc;
    u64 i;
    size_t name_len;

    if (name == (const char *)0) {
        return (char *)0;
    }
    if (strcmp(name, "NO_CONFIG") == 0) {
        return "1";
    }
    name_len = strlen(name);
    envc = cleonos_sys_proc_envc();
    for (i = 0ULL; i < envc; i++) {
        value[0] = '\0';
        if (cleonos_sys_proc_env(i, value, (u64)sizeof(value)) == 0ULL) {
            continue;
        }
        if (strncmp(value, name, name_len) == 0 && value[name_len] == '=') {
            return value + name_len + 1U;
        }
    }
    return (char *)0;
}

char *ff_cl_setlocale(int category, const char *locale) {
    static char locale_buf[CLEONOS_LOCALE_TEXT_MAX];
    (void)category;
    if (locale != (const char *)0 && locale[0] != '\0') {
        strncpy(locale_buf, locale, sizeof(locale_buf) - 1U);
        locale_buf[sizeof(locale_buf) - 1U] = '\0';
    } else if (cleonos_sys_locale_get(locale_buf, (u64)sizeof(locale_buf)) == 0ULL) {
        strcpy(locale_buf, "C");
    }
    return locale_buf;
}

int ff_cl_atexit(void (*func)(void)) {
    (void)func;
    return 0;
}

void ff_cl_exit(int status) {
    ff_cl_exit_status = status;
    longjmp(ff_cl_exit_jmp, 1);
}

int ff_cl_access(const char *path, int mode) {
    (void)mode;
    return cleonos_sys_fs_stat_type(path) != 0ULL ? 0 : -1;
}

int ff_cl_mkdir(const char *path, mode_t mode) {
    (void)mode;
    return cleonos_sys_fs_mkdir(path) != 0ULL ? 0 : -1;
}

char *ff_cl_realpath(const char *path, char *resolved) {
    if (path == (const char *)0 || resolved == (char *)0) {
        return (char *)0;
    }
    strncpy(resolved, path, PATH_MAX - 1);
    resolved[PATH_MAX - 1] = '\0';
    return resolved;
}

int ff_cl_readlink(const char *path, char *buf, size_t bufsize) {
    (void)path;
    (void)buf;
    (void)bufsize;
    return -1;
}

int ff_cl_openat(int dfd, const char *path, int flags, ...) {
    (void)dfd;
    return open(path, flags, 0644);
}

int ff_cl_dup2(int oldfd, int newfd) {
    (void)oldfd;
    (void)newfd;
    return -1;
}

int ff_cl_sigemptyset(sigset_t *set) {
    if (set != (sigset_t *)0) {
        *set = 0;
    }
    return 0;
}

int ff_cl_sigaddset(sigset_t *set, int signum) {
    (void)signum;
    if (set != (sigset_t *)0) {
        *set = 0;
    }
    return 0;
}

int ff_cl_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    (void)signum;
    (void)act;
    (void)oldact;
    return 0;
}

int ff_cl_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    (void)how;
    (void)set;
    (void)oldset;
    return 0;
}

int sigemptyset(sigset_t *set) {
    return ff_cl_sigemptyset(set);
}

int sigaddset(sigset_t *set, int signum) {
    return ff_cl_sigaddset(set, signum);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return ff_cl_sigaction(signum, act, oldact);
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    return ff_cl_sigprocmask(how, set, oldset);
}

char *dlerror(void) {
    return "dynamic loading is not available";
}

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func) {
    (void)fprintf(stderr, "fastfetch assertion failed: %s (%s:%u %s)\n",
                  expr != (const char *)0 ? expr : "?",
                  file != (const char *)0 ? file : "?",
                  line,
                  func != (const char *)0 ? func : "?");
    ff_cl_exit(1);
}

int ff_cl_tcgetattr(int fd, void *termios_p) {
    (void)fd;
    (void)termios_p;
    return -1;
}

int ff_cl_tcsetattr(int fd, int optional_actions, const void *termios_p) {
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return -1;
}

int ff_cl_gethostname(char *name, size_t len) {
    if (name == (char *)0 || len == 0U) {
        return -1;
    }
    strncpy(name, "cleonos", len - 1U);
    name[len - 1U] = '\0';
    return 0;
}

int ff_cl_clock_gettime(int clock_id, struct timespec *tp) {
    u64 ms;
    (void)clock_id;
    if (tp == (struct timespec *)0) {
        return -1;
    }
    ms = cleonos_sys_time_ms();
    tp->tv_sec = (time_t)(ms / 1000ULL);
    tp->tv_nsec = (long)((ms % 1000ULL) * 1000000ULL);
    return 0;
}

int ff_cl_nanosleep(const struct timespec *req, struct timespec *rem) {
    u64 delay_ms;
    u64 end;
    (void)rem;
    if (req == (const struct timespec *)0) {
        return -1;
    }
    delay_ms = (u64)req->tv_sec * 1000ULL + (u64)(req->tv_nsec / 1000000L);
    end = cleonos_sys_time_ms() + delay_ms;
    while (cleonos_sys_time_ms() < end) {
        (void)cleonos_sys_yield();
    }
    return 0;
}

size_t ff_cl_mbrtoc32(char32_t *out, const char *s, size_t n, mbstate_t *ps) {
    unsigned char c0;
    uint32_t cp;
    size_t len;
    (void)ps;
    if (s == (const char *)0) {
        return 0U;
    }
    if (n == 0U) {
        return (size_t)-2;
    }
    c0 = (unsigned char)s[0];
    if (c0 == 0U) {
        if (out != (char32_t *)0) {
            *out = 0U;
        }
        return 0U;
    }
    if (c0 < 0x80U) {
        cp = c0;
        len = 1U;
    } else if ((c0 & 0xE0U) == 0xC0U && n >= 2U) {
        cp = ((uint32_t)(c0 & 0x1FU) << 6U) | (uint32_t)((unsigned char)s[1] & 0x3FU);
        len = 2U;
    } else if ((c0 & 0xF0U) == 0xE0U && n >= 3U) {
        cp = ((uint32_t)(c0 & 0x0FU) << 12U) | ((uint32_t)((unsigned char)s[1] & 0x3FU) << 6U) |
             (uint32_t)((unsigned char)s[2] & 0x3FU);
        len = 3U;
    } else if ((c0 & 0xF8U) == 0xF0U && n >= 4U) {
        cp = ((uint32_t)(c0 & 0x07U) << 18U) | ((uint32_t)((unsigned char)s[1] & 0x3FU) << 12U) |
             ((uint32_t)((unsigned char)s[2] & 0x3FU) << 6U) | (uint32_t)((unsigned char)s[3] & 0x3FU);
        len = 4U;
    } else {
        return (size_t)-1;
    }
    if (out != (char32_t *)0) {
        *out = (char32_t)cp;
    }
    return len;
}

size_t ff_cl_mbrtowc(wchar_t *out, const char *s, size_t n, mbstate_t *ps) {
    char32_t c32;
    size_t len = ff_cl_mbrtoc32(&c32, s, n, ps);
    if (len < (size_t)-3 && out != (wchar_t *)0) {
        *out = (wchar_t)c32;
    }
    return len;
}

size_t mbrtowc(wchar_t *out, const char *s, size_t n, mbstate_t *ps) {
    return ff_cl_mbrtowc(out, s, n, ps);
}

unsigned int sleep(unsigned int seconds) {
    u64 end = cleonos_sys_time_ms() + (u64)seconds * 1000ULL;
    while (cleonos_sys_time_ms() < end) {
        (void)cleonos_sys_yield();
    }
    return 0U;
}
