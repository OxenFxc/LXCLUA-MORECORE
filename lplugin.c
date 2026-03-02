/*
** $Id: lplugin.c $
** Plugin system for Lua
** See Copyright Notice in lua.h
*/

#define lplugin_c
#define LUA_LIB

#include "lprefix.h"

#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "lplugin.h"

#include <ctype.h>

static void parse_and_execute(lua_State *L, const char *code) {
    const char *p = code;
    char cmd[64], arg1[256], arg2[256];

    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        cmd[0] = arg1[0] = arg2[0] = '\0';
        int n = 0;

        /* Parse command */
        while (*p && !isspace((unsigned char)*p) && n < 63) {
            cmd[n++] = *p++;
        }
        cmd[n] = '\0';

        while (isspace((unsigned char)*p) && *p != '\n') p++;

        /* Parse arg1 */
        n = 0;
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && *p != '\n' && n < 255) {
                arg1[n++] = *p++;
            }
            if (*p == '"') p++;
        } else if (*p == '\'') {
            p++;
            while (*p && *p != '\'' && *p != '\n' && n < 255) {
                arg1[n++] = *p++;
            }
            if (*p == '\'') p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && n < 255) {
                arg1[n++] = *p++;
            }
        }
        arg1[n] = '\0';

        while (isspace((unsigned char)*p) && *p != '\n') p++;

        /* Parse arg2 */
        n = 0;
        if (*p == '"') {
            p++;
            while (*p && *p != '"' && *p != '\n' && n < 255) {
                arg2[n++] = *p++;
            }
            if (*p == '"') p++;
        } else if (*p == '\'') {
            p++;
            while (*p && *p != '\'' && *p != '\n' && n < 255) {
                arg2[n++] = *p++;
            }
            if (*p == '\'') p++;
        } else {
            while (*p && *p != '\n' && n < 255) {
                arg2[n++] = *p++;
            }
        }
        /* trim trailing spaces for arg2 */
        while (n > 0 && isspace((unsigned char)arg2[n-1])) n--;
        arg2[n] = '\0';

        /* Execute */
        if (cmd[0] != '\0') {
            if (strcmp(cmd, "ADD_OPCODE") == 0) {
                /* Dynamically modify VM environment by adding opcode to registry */
                lua_getfield(L, LUA_REGISTRYINDEX, "PLUGIN_OPCODES");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_pushvalue(L, -1);
                    lua_setfield(L, LUA_REGISTRYINDEX, "PLUGIN_OPCODES");
                }
                lua_pushstring(L, arg2);
                lua_setfield(L, -2, arg1);
                lua_pop(L, 1);
            } else if (strcmp(cmd, "SET_ENV") == 0) {
                lua_pushstring(L, arg2);
                lua_setglobal(L, arg1);
            } else if (strcmp(cmd, "RUN_LUA") == 0) {
                luaL_dostring(L, arg1);
            } else {
                luaL_error(L, "plugin error: unknown command '%s'", cmd);
            }
        }

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

static int plugin_load (lua_State *L) {
  const char *plugin_code = luaL_checkstring(L, 1);
  parse_and_execute(L, plugin_code);
  lua_pushboolean(L, 1);
  return 1;
}

static const luaL_Reg pluginlib[] = {
  {"load", plugin_load},
  {NULL, NULL}
};

LUAMOD_API int luaopen_plugin (lua_State *L) {
  luaL_newlib(L, pluginlib);
  return 1;
}
