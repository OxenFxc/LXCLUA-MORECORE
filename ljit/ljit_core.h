/*
** ljit_core.h - JIT编译器主控制器
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_core_h
#define ljit_core_h

#include "ljit_types.h"
#include "ir/ljit_ir.h"
#include "opt/ljit_opt.h"
#include "emit/ljit_emit.h"
#include "mem/ljit_mem.h"
#include "trace/ljit_trace.h"

/* ========================================================================
 * JIT配置
 * ======================================================================== */

/**
 * @brief JIT配置选项
 */
typedef struct JitConfig {
    /* 启用/禁用 */
    bool enable;             /**< 是否启用JIT */
    bool enable_opt;         /**< 是否启用优化 */
    
    /* 热点阈值 */
    uint16_t hotloop;        /**< 循环热点阈值 */
    uint16_t hotcall;        /**< 调用热点阈值 */
    uint16_t hotexit;        /**< 出口热点阈值 */
    
    /* 资源限制 */
    size_t maxmcode;         /**< 最大机器码大小 */
    uint32_t maxtrace;       /**< 最大Trace数 */
    uint32_t maxrecord;      /**< 单Trace最大记录指令数 */
    uint32_t maxirconst;     /**< 最大IR常量数 */
    uint32_t maxside;        /**< 最大侧边出口数 */
    uint32_t maxsnap;        /**< 最大快照数 */
    
    /* 优化选项 */
    OptConfig opt;           /**< 优化配置 */
    
} JitConfig;

/* ========================================================================
 * JIT引擎
 * ======================================================================== */

/**
 * @brief JIT引擎
 */
typedef struct JitEngine {
    JitContext ctx;          /**< JIT上下文 */
    JitConfig config;        /**< 配置 */
    
    /* 子系统 */
    JitMem mem;              /**< 内存管理 */
    HotCountTable hotcounts; /**< 热点计数 */
    TraceRecorder recorder;  /**< Trace记录器 */
    Optimizer opt;           /**< 优化器 */
    Emitter emitter;         /**< 代码发射器 */
    
    /* 运行时 */
    void *L;                 /**< lua_State指针 */
    
    /* 统计 */
    struct {
        uint64_t trace_start;    /**< 开始记录次数 */
        uint64_t trace_abort;    /**< 中止次数 */
        uint64_t trace_success;  /**< 成功次数 */
        uint64_t trace_exec;     /**< 执行次数 */
        uint64_t exit_count;     /**< 侧边出口次数 */
        uint64_t mcode_size;     /**< 生成代码大小 */
    } stats;
    
} JitEngine;

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

/**
 * @brief 初始化JIT引擎
 * @param jit JIT引擎
 * @param L Lua状态
 * @return 成功返回JIT_OK
 */
JitError ljit_init(JitEngine *jit, void *L);

/**
 * @brief 销毁JIT引擎
 * @param jit JIT引擎
 */
void ljit_free(JitEngine *jit);

/**
 * @brief 重置JIT引擎
 * @param jit JIT引擎
 */
void ljit_reset(JitEngine *jit);

/**
 * @brief 设置默认配置
 * @param config 配置结构
 */
void ljit_config_default(JitConfig *config);

/**
 * @brief 应用配置
 * @param jit JIT引擎
 * @param config 配置
 * @return 成功返回JIT_OK
 */
JitError ljit_config_apply(JitEngine *jit, const JitConfig *config);

/* ========================================================================
 * JIT控制
 * ======================================================================== */

/**
 * @brief 启用JIT
 * @param jit JIT引擎
 */
void ljit_enable(JitEngine *jit);

/**
 * @brief 禁用JIT
 * @param jit JIT引擎
 */
void ljit_disable(JitEngine *jit);

/**
 * @brief 检查JIT是否启用
 * @param jit JIT引擎
 * @return 是否启用
 */
bool ljit_is_enabled(JitEngine *jit);

/**
 * @brief 刷新所有编译结果
 * @param jit JIT引擎
 */
void ljit_flush(JitEngine *jit);

/* ========================================================================
 * 热点检测入口
 * ======================================================================== */

/**
 * @brief 检查循环热点
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 是否需要编译
 */
bool ljit_check_hotloop(JitEngine *jit, Proto *proto, uint32_t pc);

/**
 * @brief 检查调用热点
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 是否需要编译
 */
bool ljit_check_hotcall(JitEngine *jit, Proto *proto, uint32_t pc);

/**
 * @brief 处理侧边出口
 * @param jit JIT引擎
 * @param trace Trace
 * @param exit_id 出口ID
 * @return 处理结果
 */
JitError ljit_handle_exit(JitEngine *jit, Trace *trace, uint32_t exit_id);

/* ========================================================================
 * 编译流程
 * ======================================================================== */

/**
 * @brief 开始Trace记录
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 起始PC
 * @return 成功返回JIT_OK
 */
JitError ljit_trace_start(JitEngine *jit, Proto *proto, uint32_t pc);

/**
 * @brief 记录字节码
 * @param jit JIT引擎
 * @param ins 字节码指令
 * @return 继续记录返回true
 */
bool ljit_trace_record(JitEngine *jit, uint64_t ins);

/**
 * @brief 结束并编译Trace
 * @param jit JIT引擎
 * @return 编译后的Trace，失败返回NULL
 */
Trace *ljit_trace_finish(JitEngine *jit);

/**
 * @brief 中止Trace记录
 * @param jit JIT引擎
 * @param reason 中止原因
 */
void ljit_trace_abort(JitEngine *jit, AbortReason reason);

/**
 * @brief 编译Trace
 * @param jit JIT引擎
 * @param trace Trace结构
 * @return 成功返回JIT_OK
 */
JitError ljit_compile(JitEngine *jit, Trace *trace);

/* ========================================================================
 * Trace执行
 * ======================================================================== */

/**
 * @brief 查找已编译的Trace
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 找到的Trace，未找到返回NULL
 */
Trace *ljit_find_trace(JitEngine *jit, Proto *proto, uint32_t pc);

/**
 * @brief 执行编译后的Trace
 * @param jit JIT引擎
 * @param trace Trace
 * @return 执行结果
 */
JitError ljit_execute(JitEngine *jit, Trace *trace);

/**
 * @brief 获取Trace入口点
 * @param trace Trace
 * @return 函数指针
 */
void *ljit_trace_entry(Trace *trace);

/* ========================================================================
 * 统计和调试
 * ======================================================================== */

/**
 * @brief 打印JIT统计信息
 * @param jit JIT引擎
 */
void ljit_dump_stats(JitEngine *jit);

/**
 * @brief 打印所有Trace
 * @param jit JIT引擎
 */
void ljit_dump_traces(JitEngine *jit);

/**
 * @brief 打印单个Trace
 * @param jit JIT引擎
 * @param trace_id Trace ID
 */
void ljit_dump_trace(JitEngine *jit, uint32_t trace_id);

/**
 * @brief 获取错误消息
 * @param error 错误码
 * @return 错误消息字符串
 */
const char *ljit_error_str(JitError error);

/* ========================================================================
 * VM集成钩子
 * ======================================================================== */

/**
 * @brief VM循环后向跳转钩子
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 目标PC
 * @return 如果应该执行JIT代码返回true
 */
bool ljit_vm_loop(JitEngine *jit, Proto *proto, uint32_t pc);

/**
 * @brief VM调用钩子
 * @param jit JIT引擎
 * @param proto 被调用函数
 * @return 如果应该执行JIT代码返回true
 */
bool ljit_vm_call(JitEngine *jit, Proto *proto);

/**
 * @brief VM返回钩子
 * @param jit JIT引擎
 */
void ljit_vm_return(JitEngine *jit);

#endif /* ljit_core_h */
