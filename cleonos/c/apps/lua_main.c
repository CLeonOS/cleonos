#include <cleonos_syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define CLEONOS_LUA_LINE_MAX 512U
#define CLEONOS_LUA_CHUNK_MAX 65536U

static void lua_print_usage(void) {
    puts("lua: Lua 5.4 for CLeonOS");
    puts("usage:");
    puts("  lua");
    puts("  lua -e \"print(1+2)\"");
    puts("  lua /path/script.lua [args...]");
}

static int lua_report(lua_State *L, int status) {
    const char *msg;

    if (status == LUA_OK) {
        return 0;
    }

    msg = lua_tostring(L, -1);
    if (msg == (const char *)0) {
        msg = "(error object is not a string)";
    }

    dprintf(2, "lua: %s\n", msg);
    lua_pop(L, 1);
    return 1;
}

static void lua_set_argv(lua_State *L, int argc, char **argv, int script_index) {
    int i;

    lua_newtable(L);
    for (i = 0; i < argc; i++) {
        lua_pushstring(L, (argv != (char **)0 && argv[i] != (char *)0) ? argv[i] : "");
        lua_rawseti(L, -2, i - script_index);
    }
    lua_setglobal(L, "arg");
}

static int lua_run_string(lua_State *L, const char *code) {
    int status;

    if (code == (const char *)0) {
        return 1;
    }

    status = luaL_loadbufferx(L, code, strlen(code), "=command line", "t");
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }

    return lua_report(L, status);
}

static int lua_read_file(const char *path, char **out_data, size_t *out_size) {
    u64 fd;
    char *data;
    size_t cap = 4096U;
    size_t len = 0U;

    if (path == (const char *)0 || out_data == (char **)0 || out_size == (size_t *)0) {
        return 0;
    }

    fd = cleonos_sys_fd_open(path, CLEONOS_O_RDONLY, 0ULL);
    if (fd == (u64)-1) {
        return 0;
    }

    data = (char *)malloc(cap + 1U);
    if (data == (char *)0) {
        (void)cleonos_sys_fd_close(fd);
        return 0;
    }

    for (;;) {
        u64 got;

        if (len == cap) {
            char *next;
            cap *= 2U;
            if (cap > CLEONOS_LUA_CHUNK_MAX) {
                free(data);
                (void)cleonos_sys_fd_close(fd);
                return 0;
            }
            next = (char *)realloc(data, cap + 1U);
            if (next == (char *)0) {
                free(data);
                (void)cleonos_sys_fd_close(fd);
                return 0;
            }
            data = next;
        }

        got = cleonos_sys_fd_read(fd, data + len, (u64)(cap - len));
        if (got == (u64)-1) {
            free(data);
            (void)cleonos_sys_fd_close(fd);
            return 0;
        }

        if (got == 0ULL) {
            break;
        }

        len += (size_t)got;
    }

    (void)cleonos_sys_fd_close(fd);
    data[len] = '\0';
    *out_data = data;
    *out_size = len;
    return 1;
}

static int lua_run_file(lua_State *L, const char *path) {
    char *data = (char *)0;
    size_t size = 0U;
    int status;
    char chunk_name[224];

    if (lua_read_file(path, &data, &size) == 0) {
        dprintf(2, "lua: cannot read %s\n", (path != (const char *)0) ? path : "(null)");
        return 1;
    }

    snprintf(chunk_name, (unsigned long)sizeof(chunk_name), "@%s", path);
    status = luaL_loadbufferx(L, data, size, chunk_name, "t");
    free(data);

    if (status == LUA_OK) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }

    return lua_report(L, status);
}

static int lua_repl(lua_State *L) {
    char line[CLEONOS_LUA_LINE_MAX];

    puts("Lua 5.4.8  CLeonOS minimal port");
    for (;;) {
        unsigned int len = 0U;
        int ch;

        cleonos_lua_write_fd(1, "> ", 2U);
        for (;;) {
            ch = getchar();
            if (ch == EOF) {
                putchar('\n');
                return 0;
            }
            if (ch == '\r' || ch == '\n') {
                putchar('\n');
                break;
            }
            if (ch == 8 || ch == 127) {
                if (len > 0U) {
                    len--;
                    cleonos_lua_write_fd(1, "\b \b", 3U);
                }
                continue;
            }
            if (len + 1U < sizeof(line)) {
                line[len++] = (char)ch;
                putchar(ch);
            }
        }

        line[len] = '\0';
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        if (len == 0U) {
            continue;
        }

        if (lua_run_string(L, line) != 0) {
            lua_settop(L, 0);
        }
    }

    return 0;
}

int cleonos_app_main(int argc, char **argv, char **envp) {
    lua_State *L;
    int rc;

    (void)envp;

    if (argc > 1 && argv != (char **)0 && argv[1] != (char *)0 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        lua_print_usage();
        return 0;
    }

    L = luaL_newstate();
    if (L == (lua_State *)0) {
        dprintf(2, "lua: cannot create state\n");
        return 1;
    }

    luaL_openlibs(L);

    if (argc > 2 && strcmp(argv[1], "-e") == 0) {
        lua_set_argv(L, argc, argv, 2);
        rc = lua_run_string(L, argv[2]);
    } else if (argc > 1) {
        lua_set_argv(L, argc, argv, 1);
        rc = lua_run_file(L, argv[1]);
    } else {
        lua_set_argv(L, argc, argv, 0);
        rc = lua_repl(L);
    }

    lua_close(L);
    return rc;
}
