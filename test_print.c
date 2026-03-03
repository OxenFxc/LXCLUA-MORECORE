#include "lua.h"
#include "lauxlib.h"
#include <string.h>

static int function_0(lua_State *L);

/* Proto 0 */
static int function_0(lua_State *L) {
    int vtab_idx = 3;
    lua_tcc_prologue(L, 0, 2);
    Label_1: /* VARARGPREP */
    /* VARARGPREP: adjust varargs if needed */
    Label_2: /* GETTABUP */
    lua_tcc_gettabup(L, 1, "print", 1);
    Label_3: /* LOADK */
    lua_tcc_loadk_str(L, 2, "Hello from TCC Runtime Test");
    Label_4: /* CALL */
    {
    lua_tcc_push_args(L, 1, 2); /* func + args */
    lua_call(L, 1, 0);
    lua_tcc_store_results(L, 1, 0);
    }
    Label_5: /* RETURN */
    return 0;
}

int luaopen_test_print(lua_State *L) {
    lua_pushglobaltable(L);
    lua_pushcclosure(L, function_0, 1);
    lua_call(L, 0, 1);
    return 1;
}
