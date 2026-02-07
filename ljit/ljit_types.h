/*
** ljit_types.h - JIT编译器基础类型定义
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_types_h
#define ljit_types_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct lua_State lua_State;
typedef struct Proto Proto;
typedef struct CallInfo CallInfo;

/* ========================================================================
 * JIT编译状态枚举
 * ======================================================================== */

/**
 * @brief JIT编译状态
 */
typedef enum {
    JIT_STATE_IDLE = 0,      /**< 空闲状态 */
    JIT_STATE_RECORDING,     /**< 正在记录trace */
    JIT_STATE_COMPILING,     /**< 正在编译 */
    JIT_STATE_RUNNING,       /**< 正在执行JIT代码 */
    JIT_STATE_ERROR          /**< 编译错误 */
} JitState;

/**
 * @brief 编译结果状态
 */
typedef enum {
    JIT_OK = 0,              /**< 成功 */
    JIT_ERR_MEMORY,          /**< 内存分配失败 */
    JIT_ERR_NYI,             /**< Not Yet Implemented */
    JIT_ERR_BLACKLIST,       /**< 函数被黑名单禁止 */
    JIT_ERR_TRACE_LIMIT,     /**< trace长度超限 */
    JIT_ERR_LOOP_DEPTH,      /**< 循环嵌套过深 */
    JIT_ERR_TYPE_UNSTABLE,   /**< 类型不稳定 */
    JIT_ERR_SIDE_EXIT        /**< 侧边出口过多 */
} JitError;

/* ========================================================================
 * IR类型标记
 * ======================================================================== */

/**
 * @brief IR值类型
 */
typedef enum {
    IRT_NIL = 0,
    IRT_FALSE,
    IRT_TRUE,
    IRT_INT,                 /**< lua_Integer */
    IRT_NUM,                 /**< lua_Number (double) */
    IRT_STR,                 /**< TString* */
    IRT_TAB,                 /**< Table* */
    IRT_FUNC,                /**< Closure* */
    IRT_UDATA,               /**< Udata* */
    IRT_THREAD,              /**< lua_State* */
    IRT_PTR,                 /**< 通用指针 */
    IRT_UNKNOWN              /**< 未知类型 */
} IRType;

/**
 * @brief IR操作码
 */
typedef enum {
    /* 常量与移动 */
    IR_NOP = 0,
    IR_KINT,                 /**< 整数常量 */
    IR_KNUM,                 /**< 浮点常量 */
    IR_KPTR,                 /**< 指针常量 */
    IR_KNIL,                 /**< nil常量 */
    IR_KTRUE,
    IR_KFALSE,
    IR_MOV,                  /**< 寄存器移动 */

    /* 类型守卫 */
    IR_GUARD_TYPE,           /**< 类型检查守卫 */
    IR_GUARD_NIL,
    IR_GUARD_NOTNIL,
    IR_GUARD_INT,
    IR_GUARD_NUM,
    IR_GUARD_STR,
    IR_GUARD_TAB,
    IR_GUARD_FUNC,

    /* 整数运算 */
    IR_ADD_INT,
    IR_SUB_INT,
    IR_MUL_INT,
    IR_DIV_INT,
    IR_MOD_INT,
    IR_NEG_INT,
    IR_BAND,
    IR_BOR,
    IR_BXOR,
    IR_BNOT,
    IR_SHL,
    IR_SHR,

    /* 浮点运算 */
    IR_ADD_NUM,
    IR_SUB_NUM,
    IR_MUL_NUM,
    IR_NEG_NUM,
    IR_DIV_NUM,
    IR_POW_NUM,
    IR_FLOOR,
    IR_CEIL,

    /* 类型转换 */
    IR_CONV_INT_NUM,         /**< int -> number */
    IR_CONV_NUM_INT,         /**< number -> int */
    IR_TOSTRING,
    IR_TONUMBER,

    /* 比较 */
    IR_EQ,
    IR_NE,
    IR_LT,
    IR_LE,
    IR_GT,
    IR_GE,

    /* 控制流 */
    IR_JMP,
    IR_JMPT,                 /**< 条件为真跳转 */
    IR_JMPF,                 /**< 条件为假跳转 */
    IR_LOOP,                 /**< 循环头 */
    IR_PHI,                  /**< SSA phi节点 */
    IR_RET,
    IR_RETV,                 /**< 带值返回 */

    /* 内存操作 */
    IR_LOAD,
    IR_STORE,
    IR_AREF,                 /**< 数组引用 */
    IR_HREFK,                /**< 哈希表键引用 */
    IR_HREF,                 /**< 哈希表引用 */
    IR_UREF,                 /**< upvalue引用 */

    /* 表操作 */
    IR_TGET,                 /**< 表读取 */
    IR_TSET,                 /**< 表写入 */
    IR_TNEW,                 /**< 创建新表 */
    IR_TLEN,                 /**< 表长度 */

    /* 字符串操作 */
    IR_STRCAT,               /**< 字符串拼接 */
    IR_STRLEN,

    /* 调用 */
    IR_CALL,
    IR_TAILCALL,
    IR_CALLC,                /**< C函数调用 */

    /* 杂项 */
    IR_SNAPSHOT,             /**< 快照用于去优化 */
    IR_SIDE_EXIT,            /**< 侧边出口 */

    IR__MAX
} IROp;

/* ========================================================================
 * IR指令结构
 * ======================================================================== */

/**
 * @brief IR指令引用 (16位索引)
 */
typedef uint16_t IRRef;

#define IRREF_NIL       0
#define IRREF_BIAS      0x8000  /**< 常量与变量分界 */

/**
 * @brief IR指令
 */
typedef struct IRIns {
    IROp op;                 /**< 操作码 */
    IRType type;             /**< 结果类型 */
    IRRef op1;               /**< 操作数1 */
    IRRef op2;               /**< 操作数2 */
    uint16_t prev;           /**< 前一条相同hash指令 (CSE用) */
} IRIns;

/**
 * @brief IR常量
 */
typedef union IRConst {
    int64_t i;               /**< 整数常量 */
    double n;                /**< 浮点常量 */
    void *ptr;               /**< 指针常量 */
    struct {
        uint32_t lo;
        uint32_t hi;
    } u32;
} IRConst;

/* ========================================================================
 * Trace结构
 * ======================================================================== */

/**
 * @brief Trace类型
 */
typedef enum {
    TRACE_ROOT = 0,          /**< 根trace (从循环开始) */
    TRACE_SIDE,              /**< 侧边trace */
    TRACE_STITCH             /**< 拼接trace */
} TraceType;

/**
 * @brief 侧边出口信息
 */
typedef struct SideExit {
    uint32_t pc_offset;      /**< 字节码偏移 */
    uint16_t slot_count;     /**< 需要恢复的栈槽数 */
    uint16_t snapshot_ref;   /**< 快照引用 */
} SideExit;

/**
 * @brief Trace编译结果
 */
typedef struct Trace {
    uint32_t id;             /**< Trace ID */
    TraceType type;
    Proto *proto;            /**< 所属Proto */
    uint32_t start_pc;       /**< 起始PC */

    /* IR数据 */
    IRIns *ir;               /**< IR指令数组 */
    uint32_t ir_count;       /**< IR指令数量 */
    uint32_t ir_capacity;

    /* 常量池 */
    IRConst *consts;
    uint32_t const_count;
    uint32_t const_capacity;

    /* 机器码 */
    void *mcode;             /**< 生成的机器码 */
    size_t mcode_size;

    /* 侧边出口 */
    SideExit *exits;
    uint32_t exit_count;

    /* 链接 */
    struct Trace *link;      /**< 循环链接目标 */
    struct Trace *parent;    /**< 父trace (侧边trace用) */
    uint32_t parent_exit;    /**< 从父trace的哪个出口分出 */
} Trace;

/* ========================================================================
 * 热点计数器
 * ======================================================================== */

/**
 * @brief 热点阈值配置
 */
typedef struct HotCount {
    uint16_t loop_threshold;      /**< 循环热点阈值 */
    uint16_t call_threshold;      /**< 函数调用热点阈值 */
    uint16_t side_threshold;      /**< 侧边出口热点阈值 */
} HotCount;

#define JIT_HOTLOOP_DEFAULT     56
#define JIT_HOTCALL_DEFAULT     100
#define JIT_HOTSIDE_DEFAULT     10

/* ========================================================================
 * JIT全局状态
 * ======================================================================== */

/**
 * @brief JIT编译器状态
 */
typedef struct JitContext {
    JitState state;

    /* Trace存储 */
    Trace **traces;          /**< Trace数组 */
    uint32_t trace_count;
    uint32_t trace_capacity;
    uint32_t cur_trace_id;

    /* 当前编译中的trace */
    Trace *cur_trace;
    uint32_t record_pc;      /**< 记录起点 */

    /* IR构建缓冲 */
    IRIns *ir_buf;
    uint32_t ir_cur;
    uint32_t ir_max;

    IRConst *const_buf;
    uint32_t const_cur;
    uint32_t const_max;

    /* 热点配置 */
    HotCount hotcount;

    /* 统计信息 */
    uint64_t trace_aborts;
    uint64_t trace_success;
    uint64_t mcode_total;

    /* 内存分配器 */
    struct JitMem *mem;      /**< 可执行内存管理 */

    /* 错误信息 */
    JitError last_error;
    const char *error_msg;

} JitContext;

/* ========================================================================
 * 工具宏
 * ======================================================================== */

#define JIT_ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))
#define JIT_MIN(a, b)       ((a) < (b) ? (a) : (b))
#define JIT_MAX(a, b)       ((a) > (b) ? (a) : (b))

#define irref_isconst(ref)  ((ref) < IRREF_BIAS)
#define irref_isvar(ref)    ((ref) >= IRREF_BIAS)

#endif /* ljit_types_h */
