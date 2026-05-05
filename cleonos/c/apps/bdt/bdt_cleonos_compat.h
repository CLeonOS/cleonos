#ifndef CLEONOS_BDT_COMPAT_H
#define CLEONOS_BDT_COMPAT_H

#define CLEONOS_LIBC_STDIO_H
#define CLEONOS_LIBC_TIME_H
#define BDT_PLATFORM_CLEONOS 1

#define BDT_MAX_ITEMS 128
#define BDT_MAX_CONFIG_SECTIONS 36
#define BDT_MAX_CONFIG_ITEMS 76
#define BDT_MAX_TARGETS 32
#define BDT_MAX_OUTPUT_GROUPS 6
#define BDT_MAX_APP_RULES 30
#define BDT_MAX_VARS 64
#define BDT_MAX_PLUGINS 4
#define BDT_MAX_SUBPROJECTS 8
#define BDT_MAX_BUILD_FILES 32

#include <stdarg.h>
#include <stddef.h>

typedef struct bdt_cleonos_file FILE;
typedef long time_t;
typedef long clock_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef EOF
#define EOF (-1)
#endif

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0x100
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

extern FILE *bdt_cleonos_stdin;
extern FILE *bdt_cleonos_stdout;
extern FILE *bdt_cleonos_stderr;

#define stdin bdt_cleonos_stdin
#define stdout bdt_cleonos_stdout
#define stderr bdt_cleonos_stderr

#define putchar bdt_cleonos_putchar
#define getchar bdt_cleonos_getchar
#define fputc bdt_cleonos_fputc
#define fgetc bdt_cleonos_fgetc
#define getc bdt_cleonos_getc
#define ungetc bdt_cleonos_ungetc
#define fputs bdt_cleonos_fputs
#define puts bdt_cleonos_puts
#define fflush bdt_cleonos_fflush
#define vfprintf bdt_cleonos_vfprintf
#define fprintf bdt_cleonos_fprintf
#define vprintf bdt_cleonos_vprintf
#define printf bdt_cleonos_printf
#define fopen bdt_cleonos_fopen
#define fclose bdt_cleonos_fclose
#define fread bdt_cleonos_fread
#define fwrite bdt_cleonos_fwrite
#define fgets bdt_cleonos_fgets
#define remove bdt_cleonos_remove

#define time bdt_cleonos_time
#define clock bdt_cleonos_clock
#define localtime bdt_cleonos_localtime
#define getenv bdt_cleonos_getenv
#define setenv bdt_cleonos_setenv
#define system bdt_cleonos_system
#define realpath bdt_cleonos_realpath
#define getcwd bdt_cleonos_getcwd
#define mkdir bdt_cleonos_mkdir
#define stat bdt_cleonos_stat
#define qsort bdt_cleonos_qsort

int bdt_cleonos_init(void);
void bdt_cleonos_import_env(char **envp);

int putchar(int ch);
int getchar(void);
int fputc(int ch, FILE *stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
int ungetc(int ch, FILE *stream);
int fputs(const char *text, FILE *stream);
int puts(const char *text);
int fflush(FILE *stream);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int fprintf(FILE *stream, const char *fmt, ...);
int vprintf(const char *fmt, va_list args);
int printf(const char *fmt, ...);
int vsnprintf(char *out, unsigned long out_size, const char *fmt, va_list args);
int snprintf(char *out, unsigned long out_size, const char *fmt, ...);
int sscanf(const char *text, const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *out, int size, FILE *stream);
int remove(const char *path);

time_t time(time_t *out_time);
clock_t clock(void);
struct tm *localtime(const time_t *timer);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int system(const char *cmd);
char *realpath(const char *path, char *resolved_path);
char *getcwd(char *buf, unsigned long size);
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif
