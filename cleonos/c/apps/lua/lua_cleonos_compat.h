#ifndef CLEONOS_LUA_COMPAT_H
#define CLEONOS_LUA_COMPAT_H

#include <stddef.h>
#include <stdarg.h>

#ifndef EOF
#define EOF (-1)
#endif

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct cleonos_lua_file CLEONOS_LUA_FILE;

extern CLEONOS_LUA_FILE *cleonos_lua_stdin;
extern CLEONOS_LUA_FILE *cleonos_lua_stdout;
extern CLEONOS_LUA_FILE *cleonos_lua_stderr;

CLEONOS_LUA_FILE *cleonos_lua_fopen(const char *path, const char *mode);
CLEONOS_LUA_FILE *cleonos_lua_freopen(const char *path, const char *mode, CLEONOS_LUA_FILE *stream);
int cleonos_lua_fclose(CLEONOS_LUA_FILE *stream);
size_t cleonos_lua_fread(void *out, size_t size, size_t nmemb, CLEONOS_LUA_FILE *stream);
size_t cleonos_lua_fwrite(const void *data, size_t size, size_t nmemb, CLEONOS_LUA_FILE *stream);
int cleonos_lua_fflush(CLEONOS_LUA_FILE *stream);
int cleonos_lua_ferror(CLEONOS_LUA_FILE *stream);
int cleonos_lua_feof(CLEONOS_LUA_FILE *stream);
int cleonos_lua_getc(CLEONOS_LUA_FILE *stream);
int cleonos_lua_fgetc(CLEONOS_LUA_FILE *stream);
int cleonos_lua_ungetc(int ch, CLEONOS_LUA_FILE *stream);
char *cleonos_lua_fgets(char *out, int size, CLEONOS_LUA_FILE *stream);
int cleonos_lua_fprintf(CLEONOS_LUA_FILE *stream, const char *fmt, ...);
int cleonos_lua_vfprintf(CLEONOS_LUA_FILE *stream, const char *fmt, va_list args);
int cleonos_lua_fputs(const char *text, CLEONOS_LUA_FILE *stream);
int cleonos_lua_fputc(int ch, CLEONOS_LUA_FILE *stream);
int cleonos_lua_sprintf(char *out, const char *fmt, ...);

#define FILE CLEONOS_LUA_FILE
#define stdin cleonos_lua_stdin
#define stdout cleonos_lua_stdout
#define stderr cleonos_lua_stderr
#define fopen cleonos_lua_fopen
#define freopen cleonos_lua_freopen
#define fclose cleonos_lua_fclose
#define fread cleonos_lua_fread
#define fwrite cleonos_lua_fwrite
#define fflush cleonos_lua_fflush
#define ferror cleonos_lua_ferror
#define feof cleonos_lua_feof
#define getc cleonos_lua_getc
#define fgetc cleonos_lua_fgetc
#define ungetc cleonos_lua_ungetc
#define fgets cleonos_lua_fgets
#define fprintf cleonos_lua_fprintf
#define vfprintf cleonos_lua_vfprintf
#define fputs cleonos_lua_fputs
#define fputc cleonos_lua_fputc
#define sprintf cleonos_lua_sprintf

#endif
