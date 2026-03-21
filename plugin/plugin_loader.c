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
        lua_call(L, 1, 2); /* returns table, lua_code */

        const char *lua_code = lua_tostring(L, -1);
        int code_len = strlen(lua_code);

        /* Only compile and run if there's actually Lua code after the plugin block */
        if (code_len > 0) {
            int status = luaL_loadbuffer(L, lua_code, code_len, path);
            if (status != LUA_OK) {
                free(buffer);
                return lua_error(L);
            }

            /* push metadata table as argument to the Lua script */
            lua_pushvalue(L, -3);

            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                free(buffer);
                return lua_error(L);
            }

            /* If the script returned nil, we fallback to returning the metadata table */
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                /* The metadata table is still at -2, clean up stack and leave table */
                lua_remove(L, -1); /* remove the string containing lua code */
            } else {
                /* Script returned something, remove metadata and code string from stack */
                lua_remove(L, -2);
                lua_remove(L, -2);
            }
        } else {
            /* No lua code, just return the table */
            lua_pop(L, 1); /* pop lua code string */
        }
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
