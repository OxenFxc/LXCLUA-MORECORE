#ifndef lxclua_plugin_h
#define lxclua_plugin_h

#include "../lua.h"
#include "../lauxlib.h"
#include "../lualib.h"

/*
** Main entry point for the plugin module
*/
int luaopen_plugin(lua_State *L);

/*
** Shared utilities
*/
void get_plugin_dir(char *buffer, size_t size);

/*
** Parsers and sub-modules
*/
int plugin_parse(lua_State *L);
int plugin_load(lua_State *L);
int plugin_dump(lua_State *L);
int plugin_undump(lua_State *L);
int plugin_install(lua_State *L);
int plugin_shop_connect(lua_State *L);

#endif
