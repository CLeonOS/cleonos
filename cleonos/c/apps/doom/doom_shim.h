#ifndef CLEONOS_DOOM_SHIM_H
#define CLEONOS_DOOM_SHIM_H

#include <stdio.h>
#include <stddef.h>

struct dg_stream;
typedef struct dg_stream FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#ifndef _ISupper
#define _ISupper 0x0001
#define _ISlower 0x0002
#define _ISalpha 0x0004
#define _ISdigit 0x0008
#define _ISxdigit 0x0010
#define _ISspace 0x0020
#define _ISblank 0x0040
#define _IScntrl 0x0080
#define _ISprint 0x0100
#define _ISgraph 0x0200
#define _ISalnum 0x0400
#define _ISpunct 0x0800
#endif

FILE *dg_fopen(const char *path, const char *mode);
int dg_fclose(FILE *stream);
size_t dg_fread(void *out_buf, size_t size, size_t nmemb, FILE *stream);
size_t dg_fwrite(const void *buf, size_t size, size_t nmemb, FILE *stream);
int dg_fseek(FILE *stream, long offset, int whence);
long dg_ftell(FILE *stream);
int dg_fflush(FILE *stream);
char *dg_fgets(char *out_text, int size, FILE *stream);
int dg_fprintf(FILE *stream, const char *fmt, ...);
int dg_vfprintf(FILE *stream, const char *fmt, va_list args);
int dg_feof(FILE *stream);
int dg_fileno(FILE *stream);
void dg_perror(const char *text);
int dg_sscanf(const char *text, const char *fmt, ...);

char *strdup(const char *text);
double atof(const char *text);

/* Redirect stdio-family APIs that clash with cleonos/c/src/stdio.c signatures. */
#define fopen dg_fopen
#define fclose dg_fclose
#define fread dg_fread
#define fwrite dg_fwrite
#define fseek dg_fseek
#define ftell dg_ftell
#define fflush dg_fflush
#define fgets dg_fgets
#define fprintf dg_fprintf
#define vfprintf dg_vfprintf
#define feof dg_feof
#define fileno dg_fileno
#define perror dg_perror
#define sscanf dg_sscanf

#endif
