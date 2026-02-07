/*
** ljit_lua.c - JIT与Lua VM集成层实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_lua.h"

#if LUA_USE_JIT

#include <stdlib.h>
#include <string.h>

#include "../lua.h"
#include "../lauxlib.h"
#include "../lobject.h"
#include "../lstate.h"

/* 获取JIT引擎 - 通过lua_State的userdata存储 */
static JitEngine *get_jit_engine(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "__jit_engine");
    JitEngine *jit = (JitEngine *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return jit;
}

static void set_jit_engine(lua_State *L, JitEngine *jit) {
    lua_pushlightuserdata(L, jit);
    lua_setfield(L, LUA_REGISTRYINDEX, "__jit_engine");
}

/* ========================================================================
 * VM集成API实现
 * ======================================================================== */

/**
 * @brief 为Lua状态创建JIT引擎
 * @param L Lua状态
 * @return 成功返回JIT引擎指针，失败返回NULL
 */
JitEngine *ljit_lua_create(lua_State *L) {
    JitEngine *jit = (JitEngine *)malloc(sizeof(JitEngine));
    if (!jit) return NULL;
    
    if (ljit_init(jit, L) != JIT_OK) {
        free(jit);
        return NULL;
    }
    
    return jit;
}

/**
 * @brief 销毁JIT引擎
 * @param jit JIT引擎
 */
void ljit_lua_destroy(JitEngine *jit) {
    if (jit) {
        ljit_free(jit);
        free(jit);
    }
}

/**
 * @brief VM循环后跳转钩子
 * @param jit JIT引擎
 * @param L Lua状态
 * @param proto 函数原型
 * @param pc_offset 目标PC偏移
 * @return 返回1表示应执行JIT代码
 */
int ljit_lua_loop_hook(JitEngine *jit, lua_State *L, Proto *proto, int pc_offset) {
    if (!jit || !ljit_is_enabled(jit)) return 0;
    
    uint32_t pc = (uint32_t)pc_offset;
    
    /* 查找已编译的trace */
    Trace *trace = ljit_find_trace(jit, proto, pc);
    if (trace && trace->mcode) {
        /* 执行JIT代码 */
        ljit_execute(jit, trace);
        return 1;
    }
    
    /* 检查热点 */
    if (ljit_check_hotloop(jit, proto, pc)) {
        /* 开始trace记录 */
        if (ljit_trace_start(jit, proto, pc) == JIT_OK) {
            /* 记录模式，返回0继续解释执行并记录 */
            return 0;
        }
    }
    
    return 0;
}

/**
 * @brief VM函数调用钩子
 * @param jit JIT引擎
 * @param L Lua状态
 * @param proto 被调用函数
 * @return 返回1表示应执行JIT代码
 */
int ljit_lua_call_hook(JitEngine *jit, lua_State *L, Proto *proto) {
    if (!jit || !ljit_is_enabled(jit)) return 0;
    
    return ljit_vm_call(jit, proto) ? 1 : 0;
}

/**
 * @brief 获取JIT统计信息
 * @param jit JIT引擎
 * @param L Lua状态
 * @return 压入栈的值数量
 */
int ljit_lua_getstats(JitEngine *jit, lua_State *L) {
    lua_newtable(L);
    
    lua_pushboolean(L, ljit_is_enabled(jit));
    lua_setfield(L, -2, "enabled");
    
    lua_pushinteger(L, (lua_Integer)jit->ctx.trace_count);
    lua_setfield(L, -2, "traces");
    
    lua_pushinteger(L, (lua_Integer)jit->stats.trace_success);
    lua_setfield(L, -2, "compiled");
    
    lua_pushinteger(L, (lua_Integer)jit->stats.trace_abort);
    lua_setfield(L, -2, "aborted");
    
    lua_pushinteger(L, (lua_Integer)jit->stats.trace_exec);
    lua_setfield(L, -2, "executions");
    
    lua_pushinteger(L, (lua_Integer)jit->stats.mcode_size);
    lua_setfield(L, -2, "mcode_size");
    
    return 1;
}

/**
 * @brief 设置JIT选项
 * @param jit JIT引擎
 * @param option 选项名
 * @param value 选项值
 * @return 成功返回1
 */
int ljit_lua_setopt(JitEngine *jit, const char *option, int value) {
    if (strcmp(option, "hotloop") == 0) {
        jit->config.hotloop = (uint16_t)value;
        return 1;
    }
    if (strcmp(option, "hotcall") == 0) {
        jit->config.hotcall = (uint16_t)value;
        return 1;
    }
    if (strcmp(option, "maxtrace") == 0) {
        jit->config.maxtrace = (uint32_t)value;
        return 1;
    }
    if (strcmp(option, "maxrecord") == 0) {
        jit->config.maxrecord = (uint32_t)value;
        return 1;
    }
    return 0;
}

/* ========================================================================
 * Lua库函数实现
 * ======================================================================== */

/**
 * @brief jit.on() - 启用JIT
 */
static int jit_on(lua_State *L) {
    JitEngine *jit = get_jit_engine(L);
    if (jit) ljit_enable(jit);
    return 0;
}

/**
 * @brief jit.off() - 禁用JIT
 */
static int jit_off(lua_State *L) {
    JitEngine *jit = get_jit_engine(L);
    if (jit) ljit_disable(jit);
    return 0;
}

/**
 * @brief jit.status() - 获取JIT状态
 */
static int jit_status(lua_State *L) {
    JitEngine *jit = get_jit_engine(L);
    if (!jit) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "JIT not initialized");
        return 2;
    }
    
    lua_pushboolean(L, ljit_is_enabled(jit));
    return ljit_lua_getstats(jit, L) + 1;
}

/**
 * @brief jit.flush() - 清除编译缓存
 */
static int jit_flush(lua_State *L) {
    JitEngine *jit = get_jit_engine(L);
    if (jit) ljit_flush(jit);
    return 0;
}

/**
 * @brief jit.opt.start(...) - 设置优化选项
 */
static int jit_opt_start(lua_State *L) {
    JitEngine *jit = get_jit_engine(L);
    if (!jit) return 0;
    
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        const char *opt = luaL_checkstring(L, i);
        
        /* 解析 "option=value" 格式 */
        char buf[64];
        strncpy(buf, opt, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        char *eq = strchr(buf, '=');
        if (eq) {
            *eq = '\0';
            int value = atoi(eq + 1);
            ljit_lua_setopt(jit, buf, value);
        }
    }
    
    return 0;
}

static const luaL_Reg jitlib[] = {
    {"on", jit_on},
    {"off", jit_off},
    {"status", jit_status},
    {"flush", jit_flush},
    {NULL, NULL}
};

static const luaL_Reg jitoptlib[] = {
    {"start", jit_opt_start},
    {NULL, NULL}
};

/**
 * @brief 注册JIT库到Lua
 * @param L Lua状态
 * @return 压入栈的值数量
 */
int luaopen_jit(lua_State *L) {
    /* 创建并初始化JIT引擎 */
    JitEngine *jit = ljit_lua_create(L);
    if (jit) {
        set_jit_engine(L, jit);
    }
    
    /* 创建jit表 */
    luaL_newlib(L, jitlib);
    
    /* 添加版本信息 */
    lua_pushstring(L, "0.1.0");
    lua_setfield(L, -2, "version");
    
    /* 创建jit.opt子表 */
    luaL_newlib(L, jitoptlib);
    lua_setfield(L, -2, "opt");
    
    /* 添加架构信息 */
#if defined(_M_X64) || defined(__x86_64__)
    lua_pushstring(L, "x64");
#elif defined(_M_ARM64) || defined(__aarch64__)
    lua_pushstring(L, "arm64");
#else
    lua_pushstring(L, "unknown");
#endif
    lua_setfield(L, -2, "arch");
    
    return 1;
}

#else /* !LUA_USE_JIT */

/* JIT禁用时的空实现 */
#include "../lua.h"
#include "../lauxlib.h"
#include "../lobject.h"
#include "../lstate.h"

JitEngine *ljit_lua_create(lua_State *L) {
    (void)L;
    return NULL;
}

void ljit_lua_destroy(JitEngine *jit) {
    (void)jit;
}

int ljit_lua_loop_hook(JitEngine *jit, lua_State *L, Proto *proto, int pc_offset) {
    (void)jit; (void)L; (void)proto; (void)pc_offset;
    return 0;
}

int ljit_lua_call_hook(JitEngine *jit, lua_State *L, Proto *proto) {
    (void)jit; (void)L; (void)proto;
    return 0;
}

int luaopen_jit(lua_State *L) {
    lua_newtable(L);
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "enabled");
    lua_pushstring(L, "JIT not compiled");
    lua_setfield(L, -2, "status");
    return 1;
}

#endif /* LUA_USE_JIT */
