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
#include <ctype.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lplugin.h"

/* --- Plugin VM Definition --- */

typedef enum {
    OP_NOP,
    OP_PRINT,        /* Print a string to stdout */
    OP_ADD_OPCODE,   /* Adds an opcode to registry: ADD_OPCODE "NAME" "VALUE" */
    OP_SET_GSTR,     /* Sets a string global: SET_GSTR "NAME" "VALUE" */
    OP_SET_GINT,     /* Sets an integer global: SET_GINT "NAME" 123 */
    OP_RUN,          /* Executes Lua code: RUN "print('hi')" */
    OP_HOOK_FUNC,    /* Hook a lua function: HOOK_FUNC "old_func" "new_func" */
    OP_REPLACE_FUNC, /* Replace a lua function: REPLACE_FUNC "old_func" "new_func" */
    OP_DUMP_STACK,   /* Dumps the Lua stack */
    OP_GC,           /* Forces a Lua GC collection */
    OP_HALT          /* Halts the plugin execution */
} PluginOpCode;

typedef struct {
    PluginOpCode op;
    char arg1[256];
    char arg2[256];
    int iarg;
} PluginInstruction;

typedef struct {
    PluginInstruction *code;
    int count;
    int capacity;
} PluginProgram;

static void program_init(PluginProgram *prog) {
    prog->count = 0;
    prog->capacity = 16;
    prog->code = (PluginInstruction*)malloc(sizeof(PluginInstruction) * prog->capacity);
}

static void program_add(PluginProgram *prog, PluginInstruction inst) {
    if (prog->count >= prog->capacity) {
        prog->capacity *= 2;
        prog->code = (PluginInstruction*)realloc(prog->code, sizeof(PluginInstruction) * prog->capacity);
    }
    prog->code[prog->count++] = inst;
}

static void program_free(PluginProgram *prog) {
    free(prog->code);
    prog->count = 0;
    prog->capacity = 0;
}

/* --- Lexer and Parser --- */

static const char* skip_whitespace(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char* parse_token(const char *p, char *out, int max_len) {
    p = skip_whitespace(p);
    if (*p == '\0') return p;

    /* Skip comments */
    if (*p == '#') {
        while (*p && *p != '\n') p++;
        return skip_whitespace(p);
    }

    int n = 0;
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        while (*p && *p != quote && *p != '\n' && n < max_len - 1) {
            out[n++] = *p++;
        }
        if (*p == quote) p++;
    } else {
        while (*p && !isspace((unsigned char)*p) && n < max_len - 1) {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return p;
}

static int compile_plugin(lua_State *L, const char *source, PluginProgram *prog) {
    const char *p = source;
    char token[256];

    while (*p) {
        p = skip_whitespace(p);
        if (*p == '\0') break;

        p = parse_token(p, token, sizeof(token));
        if (token[0] == '\0') continue;

        PluginInstruction inst;
        memset(&inst, 0, sizeof(PluginInstruction));

        if (strcmp(token, "PRINT") == 0) {
            inst.op = OP_PRINT;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
        } else if (strcmp(token, "ADD_OPCODE") == 0) {
            inst.op = OP_ADD_OPCODE;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
            p = parse_token(p, inst.arg2, sizeof(inst.arg2));
        } else if (strcmp(token, "SET_GSTR") == 0) {
            inst.op = OP_SET_GSTR;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
            p = parse_token(p, inst.arg2, sizeof(inst.arg2));
        } else if (strcmp(token, "SET_GINT") == 0) {
            inst.op = OP_SET_GINT;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
            char num_str[256];
            p = parse_token(p, num_str, sizeof(num_str));
            inst.iarg = atoi(num_str);
        } else if (strcmp(token, "RUN") == 0) {
            inst.op = OP_RUN;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
        } else if (strcmp(token, "HOOK_FUNC") == 0) {
            inst.op = OP_HOOK_FUNC;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
            p = parse_token(p, inst.arg2, sizeof(inst.arg2));
        } else if (strcmp(token, "REPLACE_FUNC") == 0) {
            inst.op = OP_REPLACE_FUNC;
            p = parse_token(p, inst.arg1, sizeof(inst.arg1));
            p = parse_token(p, inst.arg2, sizeof(inst.arg2));
        } else if (strcmp(token, "DUMP_STACK") == 0) {
            inst.op = OP_DUMP_STACK;
        } else if (strcmp(token, "GC") == 0) {
            inst.op = OP_GC;
        } else if (strcmp(token, "HALT") == 0) {
            inst.op = OP_HALT;
        } else {
            luaL_error(L, "Plugin Compilation Error: Unknown instruction '%s'", token);
            return 0;
        }

        program_add(prog, inst);
    }

    PluginInstruction halt_inst = {OP_HALT, "", "", 0};
    program_add(prog, halt_inst);
    return 1;
}

/* --- Virtual Machine Executor --- */

static void execute_plugin(lua_State *L, PluginProgram *prog) {
    int pc = 0;
    while (pc < prog->count) {
        PluginInstruction *inst = &prog->code[pc++];

        switch (inst->op) {
            case OP_NOP:
                break;
            case OP_PRINT:
                printf("[Plugin] %s\n", inst->arg1);
                break;
            case OP_ADD_OPCODE:
                lua_getfield(L, LUA_REGISTRYINDEX, "PLUGIN_OPCODES");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_pushvalue(L, -1);
                    lua_setfield(L, LUA_REGISTRYINDEX, "PLUGIN_OPCODES");
                }
                lua_pushstring(L, inst->arg2);
                lua_setfield(L, -2, inst->arg1);
                lua_pop(L, 1);
                break;
            case OP_SET_GSTR:
                lua_pushstring(L, inst->arg2);
                lua_setglobal(L, inst->arg1);
                break;
            case OP_SET_GINT:
                lua_pushinteger(L, inst->iarg);
                lua_setglobal(L, inst->arg1);
                break;
            case OP_RUN:
                if (luaL_dostring(L, inst->arg1) != LUA_OK) {
                    printf("[Plugin Lua Error] %s\n", lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
                break;
            case OP_HOOK_FUNC: {
                /* Replace inst->arg1 global with a hook that calls inst->arg2 then arg1 */
                lua_getglobal(L, inst->arg1);
                if (!lua_isfunction(L, -1)) {
                    lua_pop(L, 1);
                    break;
                }
                lua_getglobal(L, inst->arg2);
                if (!lua_isfunction(L, -1)) {
                    lua_pop(L, 2);
                    break;
                }
                /* create closure with original function as upvalue 1 and hook as upvalue 2 */
                /* For simplicity, we just set the new function as the global in this naive implementation.
                 * A real hook would involve C closures, but we emulate it via Lua code for the plugin VM */
                char hook_code[2048];
                sprintf(hook_code, "local old_%s = %s; %s = function(...) %s(...) return old_%s(...) end",
                        inst->arg1, inst->arg1, inst->arg1, inst->arg2, inst->arg1);
                luaL_dostring(L, hook_code);
                lua_pop(L, 2);
                break;
            }
            case OP_REPLACE_FUNC: {
                lua_getglobal(L, inst->arg2);
                if (lua_isfunction(L, -1)) {
                    lua_setglobal(L, inst->arg1);
                } else {
                    lua_pop(L, 1);
                }
                break;
            }
            case OP_DUMP_STACK: {
                int top = lua_gettop(L);
                printf("[Plugin Stack Dump] Top: %d\n", top);
                for (int i = 1; i <= top; i++) {
                    int t = lua_type(L, i);
                    printf("  %d: %s\n", i, lua_typename(L, t));
                }
                break;
            }
            case OP_GC:
                lua_gc(L, LUA_GCCOLLECT, 0);
                printf("[Plugin] Forced Garbage Collection.\n");
                break;
            case OP_HALT:
                return; /* Exit the VM */
        }
    }
}

/* --- Lua API Bindings --- */

static int plugin_load (lua_State *L) {
    const char *source = luaL_checkstring(L, 1);

    PluginProgram prog;
    program_init(&prog);

    if (compile_plugin(L, source, &prog)) {
        execute_plugin(L, &prog);
    }

    program_free(&prog);
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
