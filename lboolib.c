/*
** boolean library for Lua
** See Copyright Notice in lua.h
*/

#define lboolib_c
#define LUA_LIB

#include "lprefix.h"

#include <string.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


static int bool_to_string (lua_State *L) {
  int b = lua_toboolean(L, 1);
  lua_pushstring(L, b ? "true" : "false");
  return 1;
}


static int bool_to_number (lua_State *L) {
  int b = lua_toboolean(L, 1);
  lua_pushnumber(L, (lua_Number)(b ? 1 : 0));
  return 1;
}


static int bool_negate (lua_State *L) {
  int b = lua_toboolean(L, 1);
  lua_pushboolean(L, !b);
  return 1;
}


static int bool_and (lua_State *L) {
  int n = lua_gettop(L);
  for (int i = 1; i <= n; i++) {
    if (!lua_toboolean(L, i)) {
      lua_pushboolean(L, 0);
      return 1;
    }
  }
  lua_pushboolean(L, 1);
  return 1;
}


static int bool_or (lua_State *L) {
  int n = lua_gettop(L);
  for (int i = 1; i <= n; i++) {
    if (lua_toboolean(L, i)) {
      lua_pushboolean(L, 1);
      return 1;
    }
  }
  lua_pushboolean(L, 0);
  return 1;
}


static int bool_xor (lua_State *L) {
  int a = lua_toboolean(L, 1);
  int b = lua_toboolean(L, 2);
  lua_pushboolean(L, (a && !b) || (!a && b));
  return 1;
}


static int bool_eq (lua_State *L) {
  int a = lua_toboolean(L, 1);
  int b = lua_toboolean(L, 2);
  lua_pushboolean(L, a == b);
  return 1;
}


static int bool_is_boolean (lua_State *L) {
  lua_pushboolean(L, lua_type(L, 1) == LUA_TBOOLEAN);
  return 1;
}


/* Helper function to generate a random character */
static char random_char() {
  /* Generate a random printable ASCII character (33-126) */
  return (char)(33 + (rand() % 94));
}

/* Helper function to generate a random string */
static void random_string(char *buf, size_t len) {
  if (len == 0) return;
  
  /* Generate random length between 1 and len-1 */
  size_t str_len = 1 + (rand() % (len - 1));
  
  for (size_t i = 0; i < str_len; i++) {
    buf[i] = random_char();
  }
  
  buf[str_len] = '\0';
}

static void generate_bool_expr(luaL_Buffer *b, int target, int depth) {
  if (depth <= 0) {
    if (target) {
      luaL_addstring(b, "true");
    } else {
      luaL_addstring(b, "false");
    }
    return;
  }

  int op = rand() % 5; /* 0: and, 1: or, 2: not, 3: ==, 4: ~= */

  luaL_addchar(b, '(');

  if (op == 2) { /* not */
    luaL_addstring(b, "not ");
    generate_bool_expr(b, !target, depth - 1);
  } else {
    int left_target, right_target;
    switch (op) {
      case 0: /* and */
        if (target) {
          left_target = 1;
          right_target = 1;
        } else {
          left_target = rand() % 2;
          right_target = left_target ? 0 : (rand() % 2);
        }
        break;
      case 1: /* or */
        if (target) {
          left_target = rand() % 2;
          right_target = left_target ? (rand() % 2) : 1;
        } else {
          left_target = 0;
          right_target = 0;
        }
        break;
      case 3: /* == */
        left_target = rand() % 2;
        right_target = target ? left_target : !left_target;
        break;
      case 4: /* ~= */
        left_target = rand() % 2;
        right_target = target ? !left_target : left_target;
        break;
      default:
        left_target = target;
        right_target = target;
        break;
    }

    generate_bool_expr(b, left_target, depth - 1);

    switch (op) {
      case 0: luaL_addstring(b, " and "); break;
      case 1: luaL_addstring(b, " or "); break;
      case 3: luaL_addstring(b, " == "); break;
      case 4: luaL_addstring(b, " ~= "); break;
    }

    generate_bool_expr(b, right_target, depth - 1);
  }

  luaL_addchar(b, ')');
}

static int bool_toexpr (lua_State *L) {
  int target = lua_toboolean(L, 1);
  int depth = (int)luaL_optinteger(L, 2, 0);

  if (depth < 0) {
    depth = 0;
  } else if (depth > 15) {
    depth = 15;
  }
  
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  generate_bool_expr(&b, target, depth);
  luaL_pushresult(&b);
  return 1;
}


static const luaL_Reg bool_funcs[] = {
  {"tostring", bool_to_string},
  {"tonumber", bool_to_number},
  {"not", bool_negate},
  {"and", bool_and},
  {"or", bool_or},
  {"xor", bool_xor},
  {"eq", bool_eq},
  {"is", bool_is_boolean},
  {"toexpr", bool_toexpr},
  {NULL, NULL}
};


LUAMOD_API int luaopen_bool (lua_State *L) {
  luaL_newlib(L, bool_funcs);
  return 1;
}
