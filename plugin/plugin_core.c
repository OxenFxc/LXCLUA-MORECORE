#include "plugin.h"

static const luaL_Reg plugin_funcs[] = {
    {"install", plugin_install},
    {"parse", plugin_parse},
    {"load", plugin_load},
    {"dump", plugin_dump},
    {"undump", plugin_undump},
    {"shop_connect", plugin_shop_connect},
    {NULL, NULL}
};

/* Custom loader for .plugin files when used through require() */
static int loader_plugin(lua_State *L) {
    /* When require() calls loader, args are:
       1: module name
       2: loader data (filepath) */
    const char *filepath = luaL_checkstring(L, 2);

    /* We push the filepath as the first arg for plugin_load and call it */
    lua_pushcfunction(L, plugin_load);
    lua_pushstring(L, filepath);
    lua_call(L, 1, 1); /* plugin_load returns 1 value (the loaded table/function) */

    return 1;
}

/* Custom searcher for .plugin files */
static int searcher_plugin(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    char plugin_dir[512];
    get_plugin_dir(plugin_dir, sizeof(plugin_dir));

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s.plugin", plugin_dir, name);

    FILE *f = fopen(filepath, "r");
    if (f) {
        fclose(f);
        lua_pushcfunction(L, loader_plugin);
        lua_pushstring(L, filepath);
        /* Return the loader function and the filename */
        return 2;
    }

    lua_pushfstring(L, "\n\tno file '%s'", filepath);
    return 1;
}

/*
** Plugin initialization
*/
int luaopen_plugin(lua_State *L) {
    luaL_newlib(L, plugin_funcs);

    /* Inject searcher_plugin into package.searchers */
    if (lua_getglobal(L, "package") == LUA_TTABLE) {
        if (lua_getfield(L, -1, "searchers") == LUA_TTABLE) {
            int len = lua_rawlen(L, -1);
            lua_pushcfunction(L, searcher_plugin);
            lua_rawseti(L, -2, len + 1);
        }
        lua_pop(L, 1); /* pop searchers */
    }
    lua_pop(L, 1); /* pop package */

    return 1;
}
