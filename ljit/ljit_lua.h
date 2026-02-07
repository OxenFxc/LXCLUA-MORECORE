/*
** ljit_lua.h - JIT与Lua VM集成层
** LXCLUA-NCore JIT Module
**
** 使用方法:
** 1. 在lstate.h的global_State中添加: struct JitEngine *jit;
** 2. 在lstate.c的luaE_freeCI之后添加JIT初始化
** 3. 在lvm.c的循环后跳转处添加热点检测
*/

#ifndef ljit_lua_h
#define ljit_lua_h

#include "ljit_core.h"

/* 前向声明Lua类型 */
struct lua_State;
struct Proto;
struct CallInfo;

/* ========================================================================
 * VM集成API
 * ======================================================================== */

/**
 * @brief 为Lua状态创建JIT引擎
 * @param L Lua状态
 * @return 成功返回JIT引擎指针，失败返回NULL
 * 
 * 调用位置: lua_newstate() 之后
 */
JitEngine *ljit_lua_create(struct lua_State *L);

/**
 * @brief 销毁JIT引擎
 * @param jit JIT引擎
 * 
 * 调用位置: lua_close() 之前
 */
void ljit_lua_destroy(JitEngine *jit);

/**
 * @brief VM循环后跳转钩子
 * @param jit JIT引擎
 * @param L Lua状态
 * @param proto 函数原型
 * @param pc 目标PC偏移
 * @return 返回true表示应执行JIT代码
 * 
 * 调用位置: lvm.c OP_FORLOOP/后向JMP处
 * 
 * 示例插入点 (lvm.c约1900行附近):
 *   vmcase(OP_FORLOOP) {
 *     // 在执行跳转前检查热点
 *     #ifdef LUA_USE_JIT
 *     if (G(L)->jit && ljit_lua_loop_hook(G(L)->jit, L, cl->p, pc - cl->p->code)) {
 *       // JIT代码已执行，跳过解释执行
 *       goto jit_executed;
 *     }
 *     #endif
 *     ...原有代码...
 *   }
 */
int ljit_lua_loop_hook(JitEngine *jit, struct lua_State *L, 
                        struct Proto *proto, int pc_offset);

/**
 * @brief VM函数调用钩子
 * @param jit JIT引擎
 * @param L Lua状态
 * @param proto 被调用函数
 * @return 返回true表示应执行JIT代码
 * 
 * 调用位置: ldo.c luaD_precall() 的Lua函数分支
 */
int ljit_lua_call_hook(JitEngine *jit, struct lua_State *L,
                        struct Proto *proto);

/**
 * @brief 获取JIT统计信息
 * @param jit JIT引擎
 * @param L Lua状态 (用于创建结果表)
 * @return 压入Lua栈的值数量
 */
int ljit_lua_getstats(JitEngine *jit, struct lua_State *L);

/**
 * @brief 设置JIT选项
 * @param jit JIT引擎
 * @param option 选项名
 * @param value 选项值
 * @return 成功返回1
 */
int ljit_lua_setopt(JitEngine *jit, const char *option, int value);

/* ========================================================================
 * Lua库函数 (注册到jit表)
 * ======================================================================== */

/**
 * @brief 注册JIT库到Lua
 * @param L Lua状态
 * @return 压入栈的值数量
 * 
 * 提供以下API:
 *   jit.on()           - 启用JIT
 *   jit.off()          - 禁用JIT
 *   jit.status()       - 获取状态和统计
 *   jit.flush()        - 清除编译缓存
 *   jit.opt.start(...) - 设置优化选项
 */
int luaopen_jit(struct lua_State *L);

/* ========================================================================
 * 条件编译宏
 * ======================================================================== */

/* 
 * 在luaconf.h或编译选项中定义以启用JIT:
 *   #define LUA_USE_JIT 1
 * 
 * 禁用JIT:
 *   #define LUA_USE_JIT 0
 */

#ifndef LUA_USE_JIT
#define LUA_USE_JIT 0
#endif

/* JIT启用时的宏 */
#if LUA_USE_JIT

#define JIT_LOOP_CHECK(jit, L, proto, pc) \
    ljit_lua_loop_hook(jit, L, proto, pc)

#define JIT_CALL_CHECK(jit, L, proto) \
    ljit_lua_call_hook(jit, L, proto)

#else

#define JIT_LOOP_CHECK(jit, L, proto, pc) (0)
#define JIT_CALL_CHECK(jit, L, proto) (0)

#endif

#endif /* ljit_lua_h */
