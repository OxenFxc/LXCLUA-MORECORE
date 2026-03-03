#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int main() {
    lua_State *L = luaL_newstate();

    // Compile a tiny chunk
    luaL_loadstring(L, "print('hello from bytecode')");

    // Dump it
    // Wait, let's just use loadfile on a luac file with trailing spaces
    return 0;
}
