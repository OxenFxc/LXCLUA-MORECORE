/*
** ljit_ir.c - IR中间表示模块实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 分配IR缓冲区
 * @param builder IR构建器
 * @param size 初始大小
 * @return 成功返回JIT_OK
 */
static JitError ir_alloc_buffer(IRBuilder *builder, uint32_t size) {
    builder->ir = (IRIns *)malloc(sizeof(IRIns) * size);
    if (!builder->ir) return JIT_ERR_MEMORY;
    builder->ir_max = size;
    builder->ir_cur = 0;
    return JIT_OK;
}

/**
 * @brief 分配常量缓冲区
 * @param builder IR构建器
 * @param size 初始大小
 * @return 成功返回JIT_OK
 */
static JitError const_alloc_buffer(IRBuilder *builder, uint32_t size) {
    builder->consts = (IRConst *)malloc(sizeof(IRConst) * size);
    if (!builder->consts) return JIT_ERR_MEMORY;
    builder->const_max = size;
    builder->const_cur = 0;
    return JIT_OK;
}

/**
 * @brief 扩展IR缓冲区
 * @param builder IR构建器
 * @return 成功返回JIT_OK
 */
static JitError ir_grow(IRBuilder *builder) {
    uint32_t new_size = builder->ir_max * 2;
    if (new_size > IR_MAX_SIZE) return JIT_ERR_TRACE_LIMIT;
    
    IRIns *new_buf = (IRIns *)realloc(builder->ir, sizeof(IRIns) * new_size);
    if (!new_buf) return JIT_ERR_MEMORY;
    
    builder->ir = new_buf;
    builder->ir_max = new_size;
    return JIT_OK;
}

/**
 * @brief 扩展常量缓冲区
 * @param builder IR构建器
 * @return 成功返回JIT_OK
 */
static JitError const_grow(IRBuilder *builder) {
    uint32_t new_size = builder->const_max * 2;
    if (new_size > CONST_MAX_SIZE) return JIT_ERR_TRACE_LIMIT;
    
    IRConst *new_buf = (IRConst *)realloc(builder->consts, sizeof(IRConst) * new_size);
    if (!new_buf) return JIT_ERR_MEMORY;
    
    builder->consts = new_buf;
    builder->const_max = new_size;
    return JIT_OK;
}

/**
 * @brief CSE哈希函数
 * @param op 操作码
 * @param op1 操作数1
 * @param op2 操作数2
 * @return 哈希值
 */
static uint32_t cse_hash(IROp op, IRRef op1, IRRef op2) {
    return ((uint32_t)op << 16) ^ ((uint32_t)op1 << 8) ^ (uint32_t)op2;
}

/**
 * @brief 尝试CSE查找
 * @param builder IR构建器
 * @param op 操作码
 * @param op1 操作数1
 * @param op2 操作数2
 * @return 找到返回IR引用，否则返回IRREF_NIL
 */
static IRRef cse_find(IRBuilder *builder, IROp op, IRRef op1, IRRef op2) {
    if (!builder->cse_tab) return IRREF_NIL;
    
    uint32_t hash = cse_hash(op, op1, op2) & builder->cse_mask;
    IRRef ref = builder->cse_tab[hash];
    
    while (ref != IRREF_NIL) {
        IRIns *ir = &builder->ir[ref - IRREF_BIAS];
        if (ir->op == op && ir->op1 == op1 && ir->op2 == op2) {
            return ref;
        }
        ref = ir->prev;
    }
    return IRREF_NIL;
}

/**
 * @brief 插入CSE表
 * @param builder IR构建器
 * @param ref IR引用
 */
static void cse_insert(IRBuilder *builder, IRRef ref) {
    if (!builder->cse_tab) return;
    
    IRIns *ir = &builder->ir[ref - IRREF_BIAS];
    uint32_t hash = cse_hash(ir->op, ir->op1, ir->op2) & builder->cse_mask;
    
    ir->prev = builder->cse_tab[hash];
    builder->cse_tab[hash] = ref;
}

/* ========================================================================
 * IR构建器生命周期
 * ======================================================================== */

/**
 * @brief 初始化IR构建器
 * @param builder IR构建器指针
 * @param jit JIT上下文
 * @return JIT_OK成功，其他为错误码
 */
JitError ljit_ir_init(IRBuilder *builder, JitContext *jit) {
    memset(builder, 0, sizeof(IRBuilder));
    builder->jit = jit;
    
    JitError err = ir_alloc_buffer(builder, IR_INITIAL_SIZE);
    if (err != JIT_OK) return err;
    
    err = const_alloc_buffer(builder, CONST_INITIAL_SIZE);
    if (err != JIT_OK) {
        free(builder->ir);
        return err;
    }
    
    /* 分配CSE哈希表 */
    builder->cse_mask = 0xFF; /* 256 entries */
    builder->cse_tab = (uint16_t *)calloc(builder->cse_mask + 1, sizeof(uint16_t));
    if (!builder->cse_tab) {
        free(builder->ir);
        free(builder->consts);
        return JIT_ERR_MEMORY;
    }
    
    return JIT_OK;
}

/**
 * @brief 重置IR构建器 (开始新trace)
 * @param builder IR构建器
 */
void ljit_ir_reset(IRBuilder *builder) {
    builder->ir_cur = 0;
    builder->const_cur = 0;
    builder->trace = NULL;
    builder->loop_depth = 0;
    builder->loop_start = 0;
    
    if (builder->cse_tab) {
        memset(builder->cse_tab, 0, (builder->cse_mask + 1) * sizeof(uint16_t));
    }
}

/**
 * @brief 销毁IR构建器
 * @param builder IR构建器
 */
void ljit_ir_free(IRBuilder *builder) {
    if (builder->ir) free(builder->ir);
    if (builder->consts) free(builder->consts);
    if (builder->cse_tab) free(builder->cse_tab);
    if (builder->snapshots) free(builder->snapshots);
    if (builder->slot_types) free(builder->slot_types);
    memset(builder, 0, sizeof(IRBuilder));
}

/* ========================================================================
 * IR指令发射
 * ======================================================================== */

/**
 * @brief 内部发射函数
 */
static IRRef ir_emit_internal(IRBuilder *builder, IROp op, IRType type, 
                               IRRef op1, IRRef op2, bool use_cse) {
    /* CSE查找 */
    if (use_cse) {
        IRRef found = cse_find(builder, op, op1, op2);
        if (found != IRREF_NIL) return found;
    }
    
    /* 检查容量 */
    if (builder->ir_cur >= builder->ir_max) {
        if (ir_grow(builder) != JIT_OK) {
            return IRREF_NIL;
        }
    }
    
    /* 创建IR指令 */
    IRRef ref = IRREF_BIAS + builder->ir_cur;
    IRIns *ir = &builder->ir[builder->ir_cur++];
    ir->op = op;
    ir->type = type;
    ir->op1 = op1;
    ir->op2 = op2;
    ir->prev = IRREF_NIL;
    
    /* 插入CSE表 */
    if (use_cse) {
        cse_insert(builder, ref);
    }
    
    return ref;
}

/**
 * @brief 发射无操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @return IR引用
 */
IRRef ljit_ir_emit0(IRBuilder *builder, IROp op, IRType type) {
    return ir_emit_internal(builder, op, type, IRREF_NIL, IRREF_NIL, false);
}

/**
 * @brief 发射单操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @param op1 操作数1
 * @return IR引用
 */
IRRef ljit_ir_emit1(IRBuilder *builder, IROp op, IRType type, IRRef op1) {
    return ir_emit_internal(builder, op, type, op1, IRREF_NIL, true);
}

/**
 * @brief 发射双操作数IR指令
 * @param builder IR构建器
 * @param op 操作码
 * @param type 结果类型
 * @param op1 操作数1
 * @param op2 操作数2
 * @return IR引用
 */
IRRef ljit_ir_emit2(IRBuilder *builder, IROp op, IRType type, IRRef op1, IRRef op2) {
    return ir_emit_internal(builder, op, type, op1, op2, true);
}

/* ========================================================================
 * 常量发射
 * ======================================================================== */

/**
 * @brief 分配常量槽
 * @param builder IR构建器
 * @return 常量引用
 */
static IRRef const_alloc(IRBuilder *builder) {
    if (builder->const_cur >= builder->const_max) {
        if (const_grow(builder) != JIT_OK) {
            return IRREF_NIL;
        }
    }
    return builder->const_cur++;
}

/**
 * @brief 发射整数常量
 * @param builder IR构建器
 * @param val 整数值
 * @return IR引用
 */
IRRef ljit_ir_kint(IRBuilder *builder, int64_t val) {
    /* 查找已存在的常量 */
    for (uint32_t i = 0; i < builder->const_cur; i++) {
        if (builder->consts[i].i == val) {
            return i;
        }
    }
    
    IRRef ref = const_alloc(builder);
    if (ref == IRREF_NIL) return IRREF_NIL;
    
    builder->consts[ref].i = val;
    return ref;
}

/**
 * @brief 发射浮点常量
 * @param builder IR构建器
 * @param val 浮点值
 * @return IR引用
 */
IRRef ljit_ir_knum(IRBuilder *builder, double val) {
    for (uint32_t i = 0; i < builder->const_cur; i++) {
        if (builder->consts[i].n == val) {
            return i;
        }
    }
    
    IRRef ref = const_alloc(builder);
    if (ref == IRREF_NIL) return IRREF_NIL;
    
    builder->consts[ref].n = val;
    return ref;
}

/**
 * @brief 发射指针常量
 * @param builder IR构建器
 * @param ptr 指针值
 * @return IR引用
 */
IRRef ljit_ir_kptr(IRBuilder *builder, void *ptr) {
    for (uint32_t i = 0; i < builder->const_cur; i++) {
        if (builder->consts[i].ptr == ptr) {
            return i;
        }
    }
    
    IRRef ref = const_alloc(builder);
    if (ref == IRREF_NIL) return IRREF_NIL;
    
    builder->consts[ref].ptr = ptr;
    return ref;
}

/**
 * @brief 发射nil常量
 * @param builder IR构建器
 * @return IR引用
 */
IRRef ljit_ir_knil(IRBuilder *builder) {
    return ljit_ir_emit0(builder, IR_KNIL, IRT_NIL);
}

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
IRRef ljit_ir_guard_type(IRBuilder *builder, IRRef ref, IRType expected) {
    IRRef type_ref = ljit_ir_kint(builder, (int64_t)expected);
    return ir_emit_internal(builder, IR_GUARD_TYPE, expected, ref, type_ref, false);
}

/* ========================================================================
 * 算术运算
 * ======================================================================== */

IRRef ljit_ir_add_int(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_ADD_INT, IRT_INT, a, b);
}

IRRef ljit_ir_sub_int(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_SUB_INT, IRT_INT, a, b);
}

IRRef ljit_ir_mul_int(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_MUL_INT, IRT_INT, a, b);
}

IRRef ljit_ir_add_num(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_ADD_NUM, IRT_NUM, a, b);
}

IRRef ljit_ir_sub_num(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_SUB_NUM, IRT_NUM, a, b);
}

IRRef ljit_ir_mul_num(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_MUL_NUM, IRT_NUM, a, b);
}

IRRef ljit_ir_div_num(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_DIV_NUM, IRT_NUM, a, b);
}

/* ========================================================================
 * 比较运算
 * ======================================================================== */

IRRef ljit_ir_eq(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_EQ, IRT_TRUE, a, b);
}

IRRef ljit_ir_ne(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_NE, IRT_TRUE, a, b);
}

IRRef ljit_ir_lt(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_LT, IRT_TRUE, a, b);
}

IRRef ljit_ir_le(IRBuilder *builder, IRRef a, IRRef b) {
    return ljit_ir_emit2(builder, IR_LE, IRT_TRUE, a, b);
}

/* ========================================================================
 * 控制流
 * ======================================================================== */

IRRef ljit_ir_jmp(IRBuilder *builder, IRRef target) {
    return ir_emit_internal(builder, IR_JMP, IRT_NIL, target, IRREF_NIL, false);
}

IRRef ljit_ir_jmp_cond(IRBuilder *builder, IRRef cond, IRRef target, bool if_true) {
    IROp op = if_true ? IR_JMPT : IR_JMPF;
    return ir_emit_internal(builder, op, IRT_NIL, cond, target, false);
}

IRRef ljit_ir_loop(IRBuilder *builder) {
    builder->loop_start = builder->ir_cur;
    builder->loop_depth++;
    return ljit_ir_emit0(builder, IR_LOOP, IRT_NIL);
}

IRRef ljit_ir_phi(IRBuilder *builder, IRType type, IRRef left, IRRef right) {
    return ir_emit_internal(builder, IR_PHI, type, left, right, false);
}

/* ========================================================================
 * 内存操作
 * ======================================================================== */

IRRef ljit_ir_load(IRBuilder *builder, IRType type, IRRef base, IRRef offset) {
    return ir_emit_internal(builder, IR_LOAD, type, base, offset, false);
}

IRRef ljit_ir_store(IRBuilder *builder, IRRef base, IRRef offset, IRRef val) {
    /* 存储操作不能CSE */
    IRRef ref = ir_emit_internal(builder, IR_STORE, IRT_NIL, base, offset, false);
    /* val作为额外信息存储 - 这里简化处理 */
    (void)val;
    return ref;
}

/* ========================================================================
 * 表操作
 * ======================================================================== */

IRRef ljit_ir_tget(IRBuilder *builder, IRRef tab, IRRef key) {
    return ir_emit_internal(builder, IR_TGET, IRT_UNKNOWN, tab, key, false);
}

IRRef ljit_ir_tset(IRBuilder *builder, IRRef tab, IRRef key, IRRef val) {
    (void)val; /* 简化处理 */
    return ir_emit_internal(builder, IR_TSET, IRT_NIL, tab, key, false);
}

/* ========================================================================
 * 快照
 * ======================================================================== */

uint32_t ljit_ir_snapshot(IRBuilder *builder, uint32_t pc) {
    /* 简化实现：只记录PC */
    (void)builder;
    return pc;
}

/* ========================================================================
 * 侧边出口
 * ======================================================================== */

IRRef ljit_ir_side_exit(IRBuilder *builder, uint32_t snap_idx) {
    IRRef snap_ref = ljit_ir_kint(builder, snap_idx);
    return ir_emit_internal(builder, IR_SIDE_EXIT, IRT_NIL, snap_ref, IRREF_NIL, false);
}

/* ========================================================================
 * 调试
 * ======================================================================== */

static const char *ir_op_names[] = {
    "NOP", "KINT", "KNUM", "KPTR", "KNIL", "KTRUE", "KFALSE", "MOV",
    "GUARD_TYPE", "GUARD_NIL", "GUARD_NOTNIL", "GUARD_INT", "GUARD_NUM",
    "GUARD_STR", "GUARD_TAB", "GUARD_FUNC",
    "ADD_INT", "SUB_INT", "MUL_INT", "DIV_INT", "MOD_INT", "NEG_INT",
    "BAND", "BOR", "BXOR", "BNOT", "SHL", "SHR",
    "ADD_NUM", "SUB_NUM", "MUL_NUM", "NEG_NUM", "DIV_NUM", "POW_NUM",
    "FLOOR", "CEIL",
    "CONV_INT_NUM", "CONV_NUM_INT", "TOSTRING", "TONUMBER",
    "EQ", "NE", "LT", "LE", "GT", "GE",
    "JMP", "JMPT", "JMPF", "LOOP", "PHI", "RET", "RETV",
    "LOAD", "STORE", "AREF", "HREFK", "HREF", "UREF",
    "TGET", "TSET", "TNEW", "TLEN",
    "STRCAT", "STRLEN",
    "CALL", "TAILCALL", "CALLC",
    "SNAPSHOT", "SIDE_EXIT"
};

static const char *ir_type_names[] = {
    "nil", "false", "true", "int", "num", "str", "tab", "func",
    "udata", "thread", "ptr", "?"
};

/**
 * @brief 打印IR指令
 * @param builder IR构建器
 */
void ljit_ir_dump(IRBuilder *builder) {
    printf("=== IR Dump (%u instructions, %u constants) ===\n",
           builder->ir_cur, builder->const_cur);
    
    for (uint32_t i = 0; i < builder->ir_cur; i++) {
        IRIns *ir = &builder->ir[i];
        printf("%04u  %-12s %-6s  ", i, 
               ir->op < IR__MAX ? ir_op_names[ir->op] : "???",
               ir->type < IRT_UNKNOWN ? ir_type_names[ir->type] : "?");
        
        if (ir->op1 != IRREF_NIL) {
            if (irref_isconst(ir->op1)) {
                printf("K%u ", ir->op1);
            } else {
                printf("%04u ", ir->op1 - IRREF_BIAS);
            }
        }
        
        if (ir->op2 != IRREF_NIL) {
            if (irref_isconst(ir->op2)) {
                printf("K%u", ir->op2);
            } else {
                printf("%04u", ir->op2 - IRREF_BIAS);
            }
        }
        
        printf("\n");
    }
}
