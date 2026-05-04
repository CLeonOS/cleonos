#ifndef CLEONOS_LUA_CONFIG_H
#define CLEONOS_LUA_CONFIG_H

#include <stddef.h>
#include <stdio.h>

#define LUA_USE_C89
#define LUA_NOBUILTIN
#define LUAI_MAXCCALLS 100
#define lua_getlocaledecpoint() '.'
#define l_signalT int

#define lua_writestring(s, l) ((void)cleonos_lua_write_fd(1, (s), (l)))
#define lua_writeline() ((void)cleonos_lua_write_fd(1, "\n", 1))
#define lua_writestringerror(s, p) ((void)cleonos_lua_write_error((s), (p)))

int cleonos_lua_write_fd(int fd, const char *text, size_t len);
int cleonos_lua_write_error(const char *fmt, const char *arg);

#endif
