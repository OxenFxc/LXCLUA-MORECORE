#include "plugin.h"
#include <string.h>
#include <stdlib.h>

int plugin_dump(lua_State *L) {
    /* Expects a table {name="pkg", version="1.0"} */
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "name");
    const char *name = lua_tostring(L, -1);

    lua_getfield(L, 1, "version");
    const char *version = lua_tostring(L, -1);

    char buffer[256] = {0};
    int offset = 0;

    if (name) {
        strcpy(buffer + offset, name);
        offset += strlen(name) + 1;
    } else {
        buffer[offset++] = '\0';
    }

    if (version) {
        strcpy(buffer + offset, version);
        offset += strlen(version) + 1;
    } else {
        buffer[offset++] = '\0';
    }

    lua_pushlstring(L, buffer, offset);
    return 1;
}
