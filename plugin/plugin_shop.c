#include "plugin.h"

int plugin_shop_connect(lua_State *L) {
    const char *url = luaL_optstring(L, 1, "https://shop.lxclua.org/index.json");

    lua_getglobal(L, "require");
    lua_pushstring(L, "http");
    if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
        lua_getfield(L, -1, "get");
        lua_pushstring(L, url);

        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "body");
                if (lua_isstring(L, -1)) {
                    lua_pushstring(L, lua_tostring(L, -1));
                    return 1;
                }
            } else if (lua_isstring(L, -1)) {
                lua_pushstring(L, lua_tostring(L, -1));
                return 1;
            }
        } else {
            lua_pushfstring(L, "{\"error\": \"failed to execute http request: %s\"}", lua_tostring(L, -1));
            return 1;
        }
    } else {
        lua_pushfstring(L, "{\"error\": \"failed to load http module: %s\"}", lua_tostring(L, -1));
        return 1;
    }

    lua_pushstring(L, "{\"plugins\": []}");
    return 1;
}
