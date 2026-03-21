#include "plugin.h"
#include <string.h>

int plugin_undump(lua_State *L) {
    size_t len;
    const char *data = luaL_checklstring(L, 1, &len);

    lua_newtable(L);

    if (len > 0) {
        size_t n_len = strlen(data);
        if (n_len > 0) {
            lua_pushstring(L, data);
            lua_setfield(L, -2, "name");
        }

        if (n_len + 1 < len) {
            const char *v_data = data + n_len + 1;
            if (strlen(v_data) > 0) {
                lua_pushstring(L, v_data);
                lua_setfield(L, -2, "version");
            }
        }
    }

    return 1;
}
