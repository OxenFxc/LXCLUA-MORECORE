#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int plugin_load(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    FILE *f = fopen(path, "rb");
    if (!f) {
        lua_pushnil(L);
        lua_pushfstring(L, "cannot open %s", path);
        return 2;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return luaL_error(L, "memory allocation error");
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    /* Determine how to parse based on extension.
       If it ends with .plugin we parse it as a plugin,
       otherwise (if it's lua) we just use loadstring. */
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".plugin") == 0) {
        /* Call our custom plugin_parse natively */
        lua_pushcfunction(L, plugin_parse);
        lua_pushstring(L, buffer);
        lua_call(L, 1, 1); /* returns a table */
        /* To make it act like a module, return a function that returns the table */
        /* But simple enough to just return the parsed table for demonstration. */
    } else {
        /* Default lua parsing */
        int status = luaL_loadbuffer(L, buffer, size, path);
        if (status != LUA_OK) {
            free(buffer);
            return lua_error(L);
        }
    }

    free(buffer);
    return 1;
}
