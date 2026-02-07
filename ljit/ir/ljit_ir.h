/*
** ljit_ir.h - IR中间表示模块接口
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_ir_h
#define ljit_ir_h

#include "../ljit_types.h"

/* ========================================================================
 * IR构建器配置
 * ======================================================================== */

#define IR_INITIAL_SIZE     256
#define IR_MAX_SIZE         65536
#define CONST_INITIAL_SIZE  64
#define CONST_MAX_SIZE      4096

/* ========================================================================
 * IR构建器状态
 * ======================================================================== */

/**
 * @brief IR构建器
 */
typedef struct IRBuilder {
    JitContext *jit;         /**< JIT上下文 */
    Trace *trace;            /**< 当前trace */

    /* IR指令缓冲 */
    IRIns *ir;
    uint32_t ir_cur;         /**< 当前IR索引 */
    uint32_t ir_max;

    /* 常量表 */
    IRConst *consts;
    uint32_t const_cur;
    uint32_t const_max;

    /* CSE哈希表 */
    uint16_t *cse_tab;       /**< 公共子表达式消除哈希表 */
    uint32_t cse_mask;

    /* 快照信息 */
    uint32_t *snapshots;
    uint32_t snap_cur;
    uint32_t snap_max;

    /* 类型信息 */
    IRType *slot_types;      /**< 栈槽类型数组 */
    uint16_t slot_count;

    /* 循环信息 */
    uint32_t loop_depth;
    uint32_t loop_start;     /**< 循环起始IR索引 */

} IRBuilder;

/* ========================================================================
 * IR构建器生命周期
 * ======================================================================== */

/**
 * @brief 初始化IR构建器
 * @param builder IR构建器指针
 * @param jit JIT上下文
 * @return JIT_OK成功，其他为错误码
 */
JitError ljit_ir_init(IRBuilder *builder, JitContext *jit);

/**
 * @brief 重置IR构建器 (开始新trace)
 * @param builder IR构建器
 */
void ljit_ir_reset(IRBuilder *builder);

/**
 * @brief 销毁IR构建器
 * @param builder IR构建器
 */
void ljit_ir_free(IRBuilder *builder);

/* ========================================================================
 * IR指令发射
 * ======================================================================== */

/**
 * @brief 发射无操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @return IR引用
 */
IRRef ljit_ir_emit0(IRBuilder *builder, IROp op, IRType type);

/**
 * @brief 发射单操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @param op1 操作数1
 * @return IR引用
 */
IRRef ljit_ir_emit1(IRBuilder *builder, IROp op, IRType type, IRRef op1);

/**
 * @brief 发射双操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @param op1 操作数1
 * @param op2 操作数2
 * @return IR引用
 */
IRRef ljit_ir_emit2(IRBuilder *builder, IROp op, IRType type, IRRef op1, IRRef op2);

/* ========================================================================
 * 常量发射
 * ======================================================================== */

/**
 * @brief 发射整数常量
 * @param builder IR构建器
 * @param val 整数值
 * @return IR引用
 */
IRRef ljit_ir_kint(IRBuilder *builder, int64_t val);

/**
 * @brief 发射浮点常量
 * @param builder IR构建器
 * @param val 浮点值
 * @return IR引用
 */
IRRef ljit_ir_knum(IRBuilder *builder, double val);

/**
 * @brief 发射指针常量
 * @param builder IR构建器
 * @param ptr 指针值
 * @return IR引用
 */
IRRef ljit_ir_kptr(IRBuilder *builder, void *ptr);

/**
 * @brief 发射nil常量
 * @param builder IR构建器
 * @return IR引用
 */
IRRef ljit_ir_knil(IRBuilder *builder);

/* ========================================================================
 * 类型守卫
 * ======================================================================== */

/**
 * @brief 发射类型守卫指令
 * @param builder IR构建器
 * @param ref 被检查的IR引用
 * @param expected 期望的类型
 * @return 守卫IR引用
 */
IRRef ljit_ir_guard_type(IRBuilder *builder, IRRef ref, IRType expected);

/* ========================================================================
 * 算术运算
 * ======================================================================== */

/**
 * @brief 整数加法
 */
IRRef ljit_ir_add_int(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 整数减法
 */
IRRef ljit_ir_sub_int(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 整数乘法
 */
IRRef ljit_ir_mul_int(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 浮点加法
 */
IRRef ljit_ir_add_num(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 浮点减法
 */
IRRef ljit_ir_sub_num(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 浮点乘法
 */
IRRef ljit_ir_mul_num(IRBuilder *builder, IRRef a, IRRef b);

/**
 * @brief 浮点除法
 */
IRRef ljit_ir_div_num(IRBuilder *builder, IRRef a, IRRef b);

/* ========================================================================
 * 比较运算
 * ======================================================================== */

IRRef ljit_ir_eq(IRBuilder *builder, IRRef a, IRRef b);
IRRef ljit_ir_ne(IRBuilder *builder, IRRef a, IRRef b);
IRRef ljit_ir_lt(IRBuilder *builder, IRRef a, IRRef b);
IRRef ljit_ir_le(IRBuilder *builder, IRRef a, IRRef b);

/* ========================================================================
 * 控制流
 * ======================================================================== */

/**
 * @brief 发射无条件跳转
 * @param builder IR构建器
 * @param target 目标IR引用
 * @return IR引用
 */
IRRef ljit_ir_jmp(IRBuilder *builder, IRRef target);

/**
 * @brief 发射条件跳转
 * @param builder IR构建器
 * @param cond 条件IR引用
 * @param target 目标IR引用
 * @param if_true true时跳转还是false时跳转
 * @return IR引用
 */
IRRef ljit_ir_jmp_cond(IRBuilder *builder, IRRef cond, IRRef target, bool if_true);

/**
 * @brief 标记循环起点
 * @param builder IR构建器
 * @return IR引用
 */
IRRef ljit_ir_loop(IRBuilder *builder);

/**
 * @brief 发射PHI节点
 * @param builder IR构建器
 * @param type 类型
 * @param left 左分支值
 * @param right 右分支值
 * @return IR引用
 */
IRRef ljit_ir_phi(IRBuilder *builder, IRType type, IRRef left, IRRef right);

/* ========================================================================
 * 内存操作
 * ======================================================================== */

/**
 * @brief 发射内存加载
 * @param builder IR构建器
 * @param type 加载类型
 * @param base 基地址
 * @param offset 偏移量
 * @return IR引用
 */
IRRef ljit_ir_load(IRBuilder *builder, IRType type, IRRef base, IRRef offset);

/**
 * @brief 发射内存存储
 * @param builder IR构建器
 * @param base 基地址
 * @param offset 偏移量
 * @param val 存储值
 * @return IR引用
 */
IRRef ljit_ir_store(IRBuilder *builder, IRRef base, IRRef offset, IRRef val);

/* ========================================================================
 * 表操作
 * ======================================================================== */

/**
 * @brief 表读取
 */
IRRef ljit_ir_tget(IRBuilder *builder, IRRef tab, IRRef key);

/**
 * @brief 表写入
 */
IRRef ljit_ir_tset(IRBuilder *builder, IRRef tab, IRRef key, IRRef val);

/* ========================================================================
 * 快照
 * ======================================================================== */

/**
 * @brief 创建快照 (用于去优化)
 * @param builder IR构建器
 * @param pc 字节码PC
 * @return 快照索引
 */
uint32_t ljit_ir_snapshot(IRBuilder *builder, uint32_t pc);

/* ========================================================================
 * 侧边出口
 * ======================================================================== */

/**
 * @brief 发射侧边出口
 * @param builder IR构建器
 * @param snap_idx 快照索引
 * @return IR引用
 */
IRRef ljit_ir_side_exit(IRBuilder *builder, uint32_t snap_idx);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印IR指令
 * @param builder IR构建器
 */
void ljit_ir_dump(IRBuilder *builder);

#endif /* ljit_ir_h */
