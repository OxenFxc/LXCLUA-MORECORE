#include <stdio.h>
#include <stdlib.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int main() {
    lua_State *L = luaL_newstate();
    char buf[256];
    for (int i=0; i<256; i++) buf[i] = ' ';
    const char *code = "print('hello')";
    for(int i=0; code[i]; i++) buf[i] = code[i];

    int status = luaL_loadbuffer(L, buf, 256, "test");
    if (status != 0) {
        printf("Error: %s\n", lua_tostring(L, -1));
    } else {
        printf("Success!\n");
    }
    lua_close(L);
    return 0;
}
