/*
** $Id: lplugin.h $
** Plugin system for Lua
** See Copyright Notice in lua.h
*/

#ifndef lplugin_h
#define lplugin_h

#include "lua.h"

#define LUA_PLUGINLIBNAME "plugin"

LUAMOD_API int (luaopen_plugin) (lua_State *L);

#endif
