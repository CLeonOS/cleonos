#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static const luaL_Reg cleonos_lua_libs[] = {
    {LUA_GNAME, luaopen_base},
    {LUA_COLIBNAME, luaopen_coroutine},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_UTF8LIBNAME, luaopen_utf8},
    {LUA_DBLIBNAME, luaopen_debug},
    {NULL, NULL},
};

void luaL_openlibs(lua_State *L) {
    const luaL_Reg *lib;

    for (lib = cleonos_lua_libs; lib->func != NULL; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
}
