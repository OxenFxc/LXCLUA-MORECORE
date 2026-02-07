/*
** ljit_trace.h - Trace记录与热点检测
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_trace_h
#define ljit_trace_h

#include "../ljit_types.h"
#include "../ir/ljit_ir.h"

/* ========================================================================
 * 热点计数器配置
 * ======================================================================== */

#define HOTCOUNT_SIZE       64      /**< 热点计数器表大小 */
#define HOTCOUNT_PCMASK     0x3F    /**< PC哈希掩码 */

/* 默认阈值 */
#define DEFAULT_LOOP_THRESH   56
#define DEFAULT_CALL_THRESH   100
#define DEFAULT_EXIT_THRESH   10

/* ========================================================================
 * Trace记录状态
 * ======================================================================== */

/**
 * @brief 记录状态
 */
typedef enum {
    REC_IDLE = 0,            /**< 空闲 */
    REC_ACTIVE,              /**< 正在记录 */
    REC_DONE,                /**< 记录完成 */
    REC_ABORT                /**< 记录中止 */
} RecordState;

/**
 * @brief 记录中止原因
 */
typedef enum {
    ABORT_NONE = 0,
    ABORT_MAX_INS,           /**< 指令数超限 */
    ABORT_MAX_DEPTH,         /**< 嵌套深度超限 */
    ABORT_NYI,               /**< 不支持的指令 */
    ABORT_LOOP_UNROLL,       /**< 无法展开循环 */
    ABORT_BLACKLIST,         /**< 函数被黑名单 */
    ABORT_TYPE_UNSTABLE,     /**< 类型不稳定 */
    ABORT_INNER_LOOP,        /**< 内层循环 */
    ABORT_BAD_INSTR          /**< 无效指令 */
} AbortReason;

/* ========================================================================
 * 热点检测器
 * ======================================================================== */

/**
 * @brief 热点计数器表
 */
typedef struct HotCountTable {
    uint16_t loop_counts[HOTCOUNT_SIZE];   /**< 循环热点计数 */
    uint16_t call_counts[HOTCOUNT_SIZE];   /**< 调用热点计数 */
    uint16_t exit_counts[HOTCOUNT_SIZE];   /**< 出口热点计数 */
    
    uint16_t loop_thresh;    /**< 循环阈值 */
    uint16_t call_thresh;    /**< 调用阈值 */
    uint16_t exit_thresh;    /**< 出口阈值 */
} HotCountTable;

/**
 * @brief 快照槽位
 */
typedef struct SnapSlot {
    IRRef ref;               /**< 值的IR引用 */
    uint16_t slot;           /**< Lua栈槽位 */
    IRType type;             /**< 类型 */
} SnapSlot;

/**
 * @brief 快照
 */
typedef struct Snapshot {
    uint32_t pc;             /**< 字节码PC */
    uint16_t nslots;         /**< 槽位数 */
    uint16_t ref;            /**< 起始槽位引用 */
    uint16_t count;          /**< 活跃槽位数 */
    uint16_t topslot;        /**< 最高使用槽位 */
} Snapshot;

/**
 * @brief Trace记录器
 */
typedef struct TraceRecorder {
    JitContext *jit;         /**< JIT上下文 */
    IRBuilder *builder;      /**< IR构建器 */
    
    RecordState state;       /**< 记录状态 */
    AbortReason abort_reason;/**< 中止原因 */
    
    /* 当前记录的Trace */
    Trace *trace;
    
    /* 记录位置 */
    Proto *proto;            /**< 当前函数 */
    uint32_t start_pc;       /**< 起始PC */
    uint32_t cur_pc;         /**< 当前PC */
    uint32_t bc_count;       /**< 记录的字节码数 */
    uint32_t max_bc;         /**< 最大字节码数 */
    
    /* 嵌套信息 */
    uint32_t depth;          /**< 调用深度 */
    uint32_t max_depth;      /**< 最大深度 */
    
    /* 循环信息 */
    uint32_t loop_start;     /**< 循环起始PC */
    bool in_loop;            /**< 是否在循环中 */
    uint32_t loop_iter;      /**< 循环迭代次数 */
    
    /* 类型信息 */
    IRType *slot_types;      /**< 栈槽类型 */
    uint16_t max_slots;
    
    /* 快照 */
    Snapshot *snaps;
    uint32_t snap_count;
    uint32_t snap_capacity;
    
    SnapSlot *snap_slots;
    uint32_t slot_count;
    uint32_t slot_capacity;
    
    /* 常量表 */
    uint32_t const_base;     /**< 常量基址 */
    
} TraceRecorder;

/* ========================================================================
 * 热点检测
 * ======================================================================== */

/**
 * @brief 初始化热点计数器
 * @param table 计数器表
 */
void ljit_hotcount_init(HotCountTable *table);

/**
 * @brief 重置热点计数器
 * @param table 计数器表
 */
void ljit_hotcount_reset(HotCountTable *table);

/**
 * @brief 检查循环热点
 * @param table 计数器表
 * @param pc 字节码PC
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_loop(HotCountTable *table, uint32_t pc);

/**
 * @brief 检查调用热点
 * @param table 计数器表
 * @param pc 字节码PC
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_call(HotCountTable *table, uint32_t pc);

/**
 * @brief 检查出口热点
 * @param table 计数器表
 * @param exit_id 出口ID
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_exit(HotCountTable *table, uint32_t exit_id);

/**
 * @brief 设置热点阈值
 * @param table 计数器表
 * @param loop_thresh 循环阈值
 * @param call_thresh 调用阈值
 * @param exit_thresh 出口阈值
 */
void ljit_hotcount_set_thresh(HotCountTable *table,
                               uint16_t loop_thresh,
                               uint16_t call_thresh,
                               uint16_t exit_thresh);

/* ========================================================================
 * Trace记录器生命周期
 * ======================================================================== */

/**
 * @brief 初始化Trace记录器
 * @param rec 记录器
 * @param jit JIT上下文
 * @return 成功返回JIT_OK
 */
JitError ljit_rec_init(TraceRecorder *rec, JitContext *jit);

/**
 * @brief 销毁Trace记录器
 * @param rec 记录器
 */
void ljit_rec_free(TraceRecorder *rec);

/**
 * @brief 重置记录器状态
 * @param rec 记录器
 */
void ljit_rec_reset(TraceRecorder *rec);

/* ========================================================================
 * Trace记录控制
 * ======================================================================== */

/**
 * @brief 开始记录Trace
 * @param rec 记录器
 * @param proto 函数原型
 * @param pc 起始PC
 * @return 成功返回JIT_OK
 */
JitError ljit_rec_start(TraceRecorder *rec, Proto *proto, uint32_t pc);

/**
 * @brief 记录单条字节码
 * @param rec 记录器
 * @param ins 字节码指令
 * @return 继续记录返回true，终止返回false
 */
bool ljit_rec_ins(TraceRecorder *rec, uint64_t ins);

/**
 * @brief 结束记录Trace
 * @param rec 记录器
 * @return 生成的Trace，失败返回NULL
 */
Trace *ljit_rec_end(TraceRecorder *rec);

/**
 * @brief 中止记录
 * @param rec 记录器
 * @param reason 中止原因
 */
void ljit_rec_abort(TraceRecorder *rec, AbortReason reason);

/* ========================================================================
 * 字节码到IR转换
 * ======================================================================== */

/**
 * @brief 记录移动指令
 * @param rec 记录器
 * @param dst 目标槽位
 * @param src 源槽位
 */
void ljit_rec_mov(TraceRecorder *rec, uint16_t dst, uint16_t src);

/**
 * @brief 记录算术指令
 * @param rec 记录器
 * @param op IR操作码
 * @param dst 目标槽位
 * @param src1 源槽位1
 * @param src2 源槽位2
 */
void ljit_rec_arith(TraceRecorder *rec, IROp op,
                     uint16_t dst, uint16_t src1, uint16_t src2);

/**
 * @brief 记录比较指令
 * @param rec 记录器
 * @param op 比较操作
 * @param a 操作数1槽位
 * @param b 操作数2槽位
 * @return 结果IR引用
 */
IRRef ljit_rec_comp(TraceRecorder *rec, IROp op, uint16_t a, uint16_t b);

/**
 * @brief 记录跳转指令
 * @param rec 记录器
 * @param target 目标PC
 * @param cond 条件IR引用 (无条件跳转为IRREF_NIL)
 */
void ljit_rec_jump(TraceRecorder *rec, uint32_t target, IRRef cond);

/**
 * @brief 记录循环
 * @param rec 记录器
 * @return 是否成功闭合循环
 */
bool ljit_rec_loop(TraceRecorder *rec);

/**
 * @brief 记录调用
 * @param rec 记录器
 * @param func 函数槽位
 * @param nargs 参数数量
 * @param nresults 返回值数量
 */
void ljit_rec_call(TraceRecorder *rec, uint16_t func,
                    uint16_t nargs, uint16_t nresults);

/**
 * @brief 记录返回
 * @param rec 记录器
 * @param base 返回值起始槽位
 * @param nresults 返回值数量
 */
void ljit_rec_ret(TraceRecorder *rec, uint16_t base, uint16_t nresults);

/* ========================================================================
 * 快照管理
 * ======================================================================== */

/**
 * @brief 创建快照
 * @param rec 记录器
 * @param pc 当前PC
 * @return 快照索引
 */
uint32_t ljit_rec_snapshot(TraceRecorder *rec, uint32_t pc);

/**
 * @brief 添加快照槽位
 * @param rec 记录器
 * @param snap_idx 快照索引
 * @param slot Lua栈槽位
 * @param ref IR引用
 * @param type 类型
 */
void ljit_rec_snap_slot(TraceRecorder *rec, uint32_t snap_idx,
                         uint16_t slot, IRRef ref, IRType type);

/* ========================================================================
 * 类型追踪
 * ======================================================================== */

/**
 * @brief 获取槽位类型
 * @param rec 记录器
 * @param slot 槽位
 * @return 类型
 */
IRType ljit_rec_get_type(TraceRecorder *rec, uint16_t slot);

/**
 * @brief 设置槽位类型
 * @param rec 记录器
 * @param slot 槽位
 * @param type 类型
 */
void ljit_rec_set_type(TraceRecorder *rec, uint16_t slot, IRType type);

/**
 * @brief 发射类型守卫
 * @param rec 记录器
 * @param slot 槽位
 * @param expected 期望类型
 * @return 守卫IR引用
 */
IRRef ljit_rec_guard_type(TraceRecorder *rec, uint16_t slot, IRType expected);

/* ========================================================================
 * 黑名单管理
 * ======================================================================== */

/**
 * @brief 将函数加入黑名单
 * @param rec 记录器
 * @param proto 函数原型
 * @param pc 位置
 */
void ljit_rec_blacklist(TraceRecorder *rec, Proto *proto, uint32_t pc);

/**
 * @brief 检查是否在黑名单中
 * @param rec 记录器
 * @param proto 函数原型
 * @param pc 位置
 * @return 是否在黑名单中
 */
bool ljit_rec_is_blacklisted(TraceRecorder *rec, Proto *proto, uint32_t pc);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印记录状态
 * @param rec 记录器
 */
void ljit_rec_dump(TraceRecorder *rec);

/**
 * @brief 获取中止原因字符串
 * @param reason 中止原因
 * @return 原因字符串
 */
const char *ljit_abort_reason_str(AbortReason reason);

#endif /* ljit_trace_h */
