#ifndef CLEONOS_FASTFETCH_COMPAT_H
#define CLEONOS_FASTFETCH_COMPAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <wchar.h>
#include <uchar.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define YYJSON_DISABLE_INCR_READER 1
#define YYJSON_DISABLE_UTILS 1
#define YYJSON_DISABLE_FAST_FP_CONV 1

#ifndef static_assert
#define static_assert _Static_assert
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifndef CLEONOS_LOCALE_TEXT_MAX
#define CLEONOS_LOCALE_TEXT_MAX 32
#endif

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

#ifndef NAME_MAX
#define NAME_MAX 96
#endif

#ifndef F_OK
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x10000
#endif
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef S_IRWXU
#define S_IRWXU 0700
#endif
#ifndef S_IRGRP
#define S_IRGRP 0040
#endif
#ifndef S_IROTH
#define S_IROTH 0004
#endif

#ifndef _IONBF
#define _IONBF 2
#define _IOFBF 0
#endif

typedef unsigned int tcflag_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif

typedef struct ff_cl_file {
    int fd;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
extern jmp_buf ff_cl_exit_jmp;
extern int ff_cl_exit_status;

int ff_cl_fputs(const char *text, FILE *stream);
int ff_cl_fputc(int ch, FILE *stream);
int ff_cl_puts(const char *text);
int ff_cl_putchar(int ch);
int ff_cl_printf(const char *fmt, ...);
int ff_cl_vprintf(const char *fmt, va_list args);
int ff_cl_fprintf(FILE *stream, const char *fmt, ...);
int ff_cl_vfprintf(FILE *stream, const char *fmt, va_list args);
size_t ff_cl_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int ff_cl_sprintf(char *out, const char *fmt, ...);
int ff_cl_vasprintf(char **out, const char *fmt, va_list args);
char *ff_cl_strcasestr(const char *haystack, const char *needle);
void ff_cl_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
int ff_cl_system(const char *command);
int ff_cl_fflush(FILE *stream);
int ff_cl_setvbuf(FILE *stream, char *buf, int mode, size_t size);
FILE *ff_cl_fopen(const char *path, const char *mode);
int ff_cl_fclose(FILE *stream);
size_t ff_cl_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int ff_cl_fseek(FILE *stream, long offset, int whence);
long ff_cl_ftell(FILE *stream);
int ff_cl_feof(FILE *stream);
int ff_cl_fileno(FILE *stream);

char *ff_cl_getenv(const char *name);
char *ff_cl_setlocale(int category, const char *locale);
int ff_cl_atexit(void (*func)(void));
void ff_cl_exit(int status);
int ff_cl_access(const char *path, int mode);
int ff_cl_mkdir(const char *path, mode_t mode);
char *ff_cl_realpath(const char *path, char *resolved);
int ff_cl_readlink(const char *path, char *buf, size_t bufsize);
int ff_cl_openat(int dfd, const char *path, int flags, ...);
int ff_cl_dup2(int oldfd, int newfd);
int ff_cl_sigemptyset(sigset_t *set);
int ff_cl_sigaddset(sigset_t *set, int signum);
int ff_cl_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int ff_cl_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int ff_cl_tcgetattr(int fd, void *termios_p);
int ff_cl_tcsetattr(int fd, int optional_actions, const void *termios_p);
int ff_cl_gethostname(char *name, size_t len);
int ff_cl_clock_gettime(int clock_id, struct timespec *tp);
int ff_cl_nanosleep(const struct timespec *req, struct timespec *rem);
size_t ff_cl_mbrtowc(wchar_t *out, const char *s, size_t n, mbstate_t *ps);
unsigned int sleep(unsigned int seconds);

#define fputs ff_cl_fputs
#define fputc ff_cl_fputc
#define puts ff_cl_puts
#define putchar ff_cl_putchar
#define printf ff_cl_printf
#define vprintf ff_cl_vprintf
#define fprintf ff_cl_fprintf
#define vfprintf ff_cl_vfprintf
#define sprintf ff_cl_sprintf
#define vasprintf ff_cl_vasprintf
#define strcasestr ff_cl_strcasestr
#define qsort ff_cl_qsort
#define system ff_cl_system
#define fwrite ff_cl_fwrite
#define fflush ff_cl_fflush
#define setvbuf ff_cl_setvbuf
#define fopen ff_cl_fopen
#define fclose ff_cl_fclose
#define fread ff_cl_fread
#define fseek ff_cl_fseek
#define ftell ff_cl_ftell
#define feof ff_cl_feof
#define fileno ff_cl_fileno
#define getenv ff_cl_getenv
#define setlocale ff_cl_setlocale
#define atexit ff_cl_atexit
#define exit ff_cl_exit
#define access ff_cl_access
#define mkdir ff_cl_mkdir
#define realpath ff_cl_realpath
#define readlink ff_cl_readlink
#define openat ff_cl_openat
#define dup2 ff_cl_dup2
#define tcgetattr ff_cl_tcgetattr
#define tcsetattr ff_cl_tcsetattr
#define gethostname ff_cl_gethostname
#define clock_gettime ff_cl_clock_gettime
#define nanosleep ff_cl_nanosleep
#define mbrtowc ff_cl_mbrtowc

#endif
