/*
** ljit.h - JIT模块统一头文件
** LXCLUA-NCore JIT Module
**
** 使用方法:
**   #include "ljit/ljit.h"
**
** 编译命令示例 (GCC):
**   gcc -std=c23 -O2 -I.. ljit/*.c ljit/ir/*.c ljit/opt/*.c 
**       ljit/emit/*.c ljit/mem/*.c ljit/trace/*.c -c
*/

#ifndef ljit_h
#define ljit_h

/* 核心类型定义 */
#include "ljit_types.h"

/* 主控制器 */
#include "ljit_core.h"

/* 子模块 (可选直接包含) */
/* #include "ir/ljit_ir.h" */
/* #include "opt/ljit_opt.h" */
/* #include "emit/ljit_emit.h" */
/* #include "mem/ljit_mem.h" */
/* #include "trace/ljit_trace.h" */

/* ========================================================================
 * 版本信息
 * ======================================================================== */

#define LJIT_VERSION_MAJOR  0
#define LJIT_VERSION_MINOR  1
#define LJIT_VERSION_PATCH  0
#define LJIT_VERSION        "0.1.0"

/* ========================================================================
 * 便捷宏
 * ======================================================================== */

/**
 * @brief 初始化并创建JIT引擎
 * @param L Lua状态指针
 * @return JIT引擎指针，失败返回NULL
 */
#define LJIT_CREATE(L) \
    ({ \
        JitEngine *_jit = (JitEngine *)malloc(sizeof(JitEngine)); \
        if (_jit && ljit_init(_jit, L) != JIT_OK) { \
            free(_jit); \
            _jit = NULL; \
        } \
        _jit; \
    })

/**
 * @brief 销毁JIT引擎
 * @param jit JIT引擎指针
 */
#define LJIT_DESTROY(jit) \
    do { \
        if (jit) { \
            ljit_free(jit); \
            free(jit); \
        } \
    } while (0)

#endif /* ljit_h */
