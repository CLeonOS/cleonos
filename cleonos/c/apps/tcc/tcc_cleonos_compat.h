#ifndef CLEONOS_TCC_COMPAT_H
#define CLEONOS_TCC_COMPAT_H

#define CLEONOS_LIBC_STDIO_H
#define CLEONOS_LIBC_TIME_H

#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>

#ifdef fileno
#undef fileno
#endif

typedef struct cleonos_tcc_file FILE;
typedef long time_t;
typedef long clock_t;

#ifndef EOF
#define EOF (-1)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define CLOCKS_PER_SEC 1000L

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

extern FILE *cleonos_tcc_stdin;
extern FILE *cleonos_tcc_stdout;
extern FILE *cleonos_tcc_stderr;

#define stdin cleonos_tcc_stdin
#define stdout cleonos_tcc_stdout
#define stderr cleonos_tcc_stderr
#define putchar cleonos_tcc_putchar
#define getchar cleonos_tcc_getchar
#define fputc cleonos_tcc_fputc
#define fgetc cleonos_tcc_fgetc
#define getc cleonos_tcc_getc
#define ungetc cleonos_tcc_ungetc
#define fputs cleonos_tcc_fputs
#define puts cleonos_tcc_puts
#define fflush cleonos_tcc_fflush
#define sprintf cleonos_tcc_sprintf
#define vfprintf cleonos_tcc_vfprintf
#define fprintf cleonos_tcc_fprintf
#define vprintf cleonos_tcc_vprintf
#define printf cleonos_tcc_printf
#define fopen cleonos_tcc_fopen
#define fdopen cleonos_tcc_fdopen
#define fclose cleonos_tcc_fclose
#define fread cleonos_tcc_fread
#define fwrite cleonos_tcc_fwrite
#define fseek cleonos_tcc_fseek
#define ftell cleonos_tcc_ftell
#define fileno cleonos_tcc_fileno
#define remove cleonos_tcc_remove
#define rename cleonos_tcc_rename
#define realpath cleonos_tcc_realpath
#define time cleonos_tcc_time
#define clock cleonos_tcc_clock
#define localtime cleonos_tcc_localtime
#define getenv cleonos_tcc_getenv
#define strerror cleonos_tcc_strerror
#define atof cleonos_tcc_atof
#define strtof cleonos_tcc_strtof
#define strtold cleonos_tcc_strtold
#define ldexpl cleonos_tcc_ldexpl
#define qsort cleonos_tcc_qsort
#define open cleonos_tcc_open
#define close cleonos_tcc_close
#define read cleonos_tcc_read
#define write cleonos_tcc_write
#define lseek cleonos_tcc_lseek
#define access cleonos_tcc_access
#define getcwd cleonos_tcc_getcwd
#define unlink cleonos_tcc_unlink
#define execvp cleonos_tcc_execvp

int cleonos_tcc_main(int argc, char **argv);
int cleonos_tcc_stdio_init(void);

int putchar(int ch);
int getchar(void);
int fputc(int ch, FILE *stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
int ungetc(int ch, FILE *stream);
int fputs(const char *text, FILE *stream);
int puts(const char *text);
int fflush(FILE *stream);
int sprintf(char *out, const char *fmt, ...);
int vsnprintf(char *out, unsigned long out_size, const char *fmt, va_list args);
int snprintf(char *out, unsigned long out_size, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int fprintf(FILE *stream, const char *fmt, ...);
int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fd, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fileno(FILE *stream);
int remove(const char *path);
int rename(const char *old_path, const char *new_path);
char *realpath(const char *path, char *resolved_path);

time_t time(time_t *out_time);
clock_t clock(void);
struct tm *localtime(const time_t *timer);
char *getenv(const char *name);
const char *strerror(int errnum);
double atof(const char *text);
float strtof(const char *text, char **out_end);
long double strtold(const char *text, char **out_end);
long double ldexpl(long double value, int exp);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif
