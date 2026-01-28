/*
** $Id: lptrlib.c $
** Pointer manipulation library for Lua
** See Copyright Notice in lua.h
*/

#define lptrlib_c
#define LUA_LIB

#include "lprefix.h"

#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


/*
** Implementation of pointer library functions
*/

static int l_ptr_addr (lua_State *L) {
  const void *p = lua_topointer(L, 1);
  lua_pushlightuserdata(L, (void *)p);
  return 1;
}


static int l_ptr_add (lua_State *L) {
  const void *p = lua_topointer(L, 1);
  ptrdiff_t offset = luaL_checkinteger(L, 2);
  p = (char *)p + offset;
  lua_pushlightuserdata(L, (void *)p);
  return 1;
}


static int l_ptr_sub (lua_State *L) {
  const void *p1 = lua_topointer(L, 1);
  const void *p2 = lua_topointer(L, 2);
  ptrdiff_t diff = (char *)p1 - (char *)p2;
  lua_pushinteger(L, diff);
  return 1;
}


static int l_ptr_read (lua_State *L) {
  const void *p = lua_topointer(L, 1);
  const char *type = luaL_checkstring(L, 2);
  
  if (strcmp(type, "int") == 0) {
    lua_pushinteger(L, *(const int *)p);
  } else if (strcmp(type, "float") == 0) {
    lua_pushnumber(L, *(const float *)p);
  } else if (strcmp(type, "double") == 0) {
    lua_pushnumber(L, *(const double *)p);
  } else if (strcmp(type, "char") == 0) {
    lua_pushinteger(L, *(const char *)p);
  } else if (strcmp(type, "unsigned int") == 0) {
    lua_pushinteger(L, *(const unsigned int *)p);
  } else if (strcmp(type, "short") == 0) {
    lua_pushinteger(L, *(const short *)p);
  } else if (strcmp(type, "long") == 0) {
    lua_pushinteger(L, *(const long *)p);
  } else if (strcmp(type, "lua_Integer") == 0) {
    lua_pushinteger(L, *(const lua_Integer *)p);
  } else if (strcmp(type, "lua_Number") == 0) {
    lua_pushnumber(L, *(const lua_Number *)p);
  } else {
    return luaL_error(L, "unsupported type for pointer read: %s", type);
  }
  
  return 1;
}


static int l_ptr_write (lua_State *L) {
  void *p = (void *)lua_topointer(L, 1);
  const char *type = luaL_checkstring(L, 2);
  
  if (strcmp(type, "int") == 0) {
    int val = luaL_checkinteger(L, 3);
    *(int *)p = val;
  } else if (strcmp(type, "float") == 0) {
    float val = (float)luaL_checknumber(L, 3);
    *(float *)p = val;
  } else if (strcmp(type, "double") == 0) {
    double val = luaL_checknumber(L, 3);
    *(double *)p = val;
  } else if (strcmp(type, "char") == 0) {
    char val = (char)luaL_checkinteger(L, 3);
    *(char *)p = val;
  } else if (strcmp(type, "unsigned int") == 0) {
    unsigned int val = (unsigned int)luaL_checkinteger(L, 3);
    *(unsigned int *)p = val;
  } else if (strcmp(type, "short") == 0) {
    short val = (short)luaL_checkinteger(L, 3);
    *(short *)p = val;
  } else if (strcmp(type, "long") == 0) {
    long val = (long)luaL_checkinteger(L, 3);
    *(long *)p = val;
  } else if (strcmp(type, "lua_Integer") == 0) {
    lua_Integer val = luaL_checkinteger(L, 3);
    *(lua_Integer *)p = val;
  } else if (strcmp(type, "lua_Number") == 0) {
    lua_Number val = luaL_checknumber(L, 3);
    *(lua_Number *)p = val;
  } else {
    return luaL_error(L, "unsupported type for pointer write: %s", type);
  }
  
  return 0;
}


static int l_ptr_malloc (lua_State *L) {
  size_t size = luaL_checkinteger(L, 1);
  void *p = lua_newuserdata(L, size);
  lua_pushlightuserdata(L, p);
  return 1;
}


static int l_ptr_free (lua_State *L) {
  /* Lua's garbage collector will handle memory freeing */
  return 0;
}


/*
** Register the ptr module functions
*/
static const luaL_Reg ptrlib[] = {
  {"addr", l_ptr_addr},
  {"add", l_ptr_add},
  {"sub", l_ptr_sub},
  {"read", l_ptr_read},
  {"write", l_ptr_write},
  {"malloc", l_ptr_malloc},
  {"free", l_ptr_free},
  {NULL, NULL}
};


/*
** Open the ptr module
*/
LUAMOD_API int luaopen_ptr (lua_State *L) {
  luaL_newlib(L, ptrlib);
  return 1;
}
