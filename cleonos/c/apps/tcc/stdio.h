#ifndef CLEONOS_TCC_STDIO_H
#define CLEONOS_TCC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#ifndef EOF
#define EOF (-1)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct cleonos_tcc_file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int putchar(int ch);
int getchar(void);
int fputc(int ch, FILE *stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
int ungetc(int ch, FILE *stream);
int fputs(const char *text, FILE *stream);
int puts(const char *text);
int fflush(FILE *stream);

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

#endif
