/*
** ljit_trace.c - Trace记录与热点检测实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 配置常量
 * ======================================================================== */

#define MAX_BC_PER_TRACE    4000     /**< 单Trace最大字节码数 */
#define MAX_CALL_DEPTH      20       /**< 最大调用深度 */
#define MAX_SNAP_COUNT      500      /**< 最大快照数 */
#define MAX_SLOT_COUNT      1000     /**< 最大槽位数 */
#define MAX_SLOTS           256      /**< 最大栈槽数 */

/* ========================================================================
 * 热点检测
 * ======================================================================== */

/**
 * @brief 初始化热点计数器
 * @param table 计数器表
 */
void ljit_hotcount_init(HotCountTable *table) {
    memset(table, 0, sizeof(HotCountTable));
    table->loop_thresh = DEFAULT_LOOP_THRESH;
    table->call_thresh = DEFAULT_CALL_THRESH;
    table->exit_thresh = DEFAULT_EXIT_THRESH;
}

/**
 * @brief 重置热点计数器
 * @param table 计数器表
 */
void ljit_hotcount_reset(HotCountTable *table) {
    memset(table->loop_counts, 0, sizeof(table->loop_counts));
    memset(table->call_counts, 0, sizeof(table->call_counts));
    memset(table->exit_counts, 0, sizeof(table->exit_counts));
}

/**
 * @brief 热点计数哈希
 */
static uint32_t hotcount_hash(uint32_t pc) {
    return pc & HOTCOUNT_PCMASK;
}

/**
 * @brief 检查循环热点
 * @param table 计数器表
 * @param pc 字节码PC
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_loop(HotCountTable *table, uint32_t pc) {
    uint32_t idx = hotcount_hash(pc);
    table->loop_counts[idx]++;
    
    if (table->loop_counts[idx] >= table->loop_thresh) {
        table->loop_counts[idx] = 0;  /* 重置避免重复触发 */
        return true;
    }
    return false;
}

/**
 * @brief 检查调用热点
 * @param table 计数器表
 * @param pc 字节码PC
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_call(HotCountTable *table, uint32_t pc) {
    uint32_t idx = hotcount_hash(pc);
    table->call_counts[idx]++;
    
    if (table->call_counts[idx] >= table->call_thresh) {
        table->call_counts[idx] = 0;
        return true;
    }
    return false;
}

/**
 * @brief 检查出口热点
 * @param table 计数器表
 * @param exit_id 出口ID
 * @return 是否达到热点阈值
 */
bool ljit_hotcount_exit(HotCountTable *table, uint32_t exit_id) {
    uint32_t idx = exit_id & HOTCOUNT_PCMASK;
    table->exit_counts[idx]++;
    
    if (table->exit_counts[idx] >= table->exit_thresh) {
        table->exit_counts[idx] = 0;
        return true;
    }
    return false;
}

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
                               uint16_t exit_thresh) {
    table->loop_thresh = loop_thresh;
    table->call_thresh = call_thresh;
    table->exit_thresh = exit_thresh;
}

/* ========================================================================
 * Trace记录器生命周期
 * ======================================================================== */

/**
 * @brief 初始化Trace记录器
 * @param rec 记录器
 * @param jit JIT上下文
 * @return 成功返回JIT_OK
 */
JitError ljit_rec_init(TraceRecorder *rec, JitContext *jit) {
    memset(rec, 0, sizeof(TraceRecorder));
    rec->jit = jit;
    rec->state = REC_IDLE;
    rec->max_bc = MAX_BC_PER_TRACE;
    rec->max_depth = MAX_CALL_DEPTH;
    
    /* 分配IR构建器 */
    rec->builder = (IRBuilder *)malloc(sizeof(IRBuilder));
    if (!rec->builder) return JIT_ERR_MEMORY;
    
    JitError err = ljit_ir_init(rec->builder, jit);
    if (err != JIT_OK) {
        free(rec->builder);
        return err;
    }
    
    /* 分配类型数组 */
    rec->slot_types = (IRType *)calloc(MAX_SLOTS, sizeof(IRType));
    if (!rec->slot_types) {
        ljit_ir_free(rec->builder);
        free(rec->builder);
        return JIT_ERR_MEMORY;
    }
    rec->max_slots = MAX_SLOTS;
    
    /* 分配快照存储 */
    rec->snap_capacity = 64;
    rec->snaps = (Snapshot *)malloc(sizeof(Snapshot) * rec->snap_capacity);
    if (!rec->snaps) {
        free(rec->slot_types);
        ljit_ir_free(rec->builder);
        free(rec->builder);
        return JIT_ERR_MEMORY;
    }
    
    rec->slot_capacity = 256;
    rec->snap_slots = (SnapSlot *)malloc(sizeof(SnapSlot) * rec->slot_capacity);
    if (!rec->snap_slots) {
        free(rec->snaps);
        free(rec->slot_types);
        ljit_ir_free(rec->builder);
        free(rec->builder);
        return JIT_ERR_MEMORY;
    }
    
    return JIT_OK;
}

/**
 * @brief 销毁Trace记录器
 * @param rec 记录器
 */
void ljit_rec_free(TraceRecorder *rec) {
    if (rec->builder) {
        ljit_ir_free(rec->builder);
        free(rec->builder);
    }
    if (rec->slot_types) free(rec->slot_types);
    if (rec->snaps) free(rec->snaps);
    if (rec->snap_slots) free(rec->snap_slots);
    memset(rec, 0, sizeof(TraceRecorder));
}

/**
 * @brief 重置记录器状态
 * @param rec 记录器
 */
void ljit_rec_reset(TraceRecorder *rec) {
    ljit_ir_reset(rec->builder);
    
    rec->state = REC_IDLE;
    rec->abort_reason = ABORT_NONE;
    rec->trace = NULL;
    rec->proto = NULL;
    rec->start_pc = 0;
    rec->cur_pc = 0;
    rec->bc_count = 0;
    rec->depth = 0;
    rec->loop_start = 0;
    rec->in_loop = false;
    rec->loop_iter = 0;
    rec->snap_count = 0;
    rec->slot_count = 0;
    
    memset(rec->slot_types, 0, sizeof(IRType) * rec->max_slots);
}

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
JitError ljit_rec_start(TraceRecorder *rec, Proto *proto, uint32_t pc) {
    if (rec->state != REC_IDLE) {
        return JIT_ERR_BLACKLIST;
    }
    
    /* 检查黑名单 */
    if (ljit_rec_is_blacklisted(rec, proto, pc)) {
        return JIT_ERR_BLACKLIST;
    }
    
    ljit_rec_reset(rec);
    
    rec->proto = proto;
    rec->start_pc = pc;
    rec->cur_pc = pc;
    rec->state = REC_ACTIVE;
    
    /* 创建新Trace */
    rec->trace = (Trace *)calloc(1, sizeof(Trace));
    if (!rec->trace) {
        rec->state = REC_IDLE;
        return JIT_ERR_MEMORY;
    }
    
    rec->trace->proto = proto;
    rec->trace->start_pc = pc;
    rec->trace->type = TRACE_ROOT;
    rec->trace->id = rec->jit->cur_trace_id++;
    
    /* 创建初始快照 */
    ljit_rec_snapshot(rec, pc);
    
    return JIT_OK;
}

/**
 * @brief 记录单条字节码
 * @param rec 记录器
 * @param ins 字节码指令
 * @return 继续记录返回true，终止返回false
 */
bool ljit_rec_ins(TraceRecorder *rec, uint64_t ins) {
    if (rec->state != REC_ACTIVE) {
        return false;
    }
    
    rec->bc_count++;
    
    /* 检查字节码限制 */
    if (rec->bc_count > rec->max_bc) {
        ljit_rec_abort(rec, ABORT_MAX_INS);
        return false;
    }
    
    /* 这里应该根据字节码类型分发处理 */
    /* 简化实现：直接返回继续记录 */
    (void)ins;
    
    return true;
}

/**
 * @brief 结束记录Trace
 * @param rec 记录器
 * @return 生成的Trace，失败返回NULL
 */
Trace *ljit_rec_end(TraceRecorder *rec) {
    if (rec->state != REC_ACTIVE) {
        return NULL;
    }
    
    Trace *trace = rec->trace;
    
    /* 复制IR数据到Trace */
    trace->ir_count = rec->builder->ir_cur;
    trace->ir = (IRIns *)malloc(sizeof(IRIns) * trace->ir_count);
    if (trace->ir) {
        memcpy(trace->ir, rec->builder->ir, sizeof(IRIns) * trace->ir_count);
    }
    
    /* 复制常量 */
    trace->const_count = rec->builder->const_cur;
    trace->consts = (IRConst *)malloc(sizeof(IRConst) * trace->const_count);
    if (trace->consts) {
        memcpy(trace->consts, rec->builder->consts, 
               sizeof(IRConst) * trace->const_count);
    }
    
    rec->trace = NULL;
    rec->state = REC_DONE;
    
    /* 添加到JIT上下文 */
    if (rec->jit->trace_count >= rec->jit->trace_capacity) {
        uint32_t new_cap = rec->jit->trace_capacity ? 
                           rec->jit->trace_capacity * 2 : 16;
        Trace **new_traces = (Trace **)realloc(rec->jit->traces,
                                                sizeof(Trace *) * new_cap);
        if (new_traces) {
            rec->jit->traces = new_traces;
            rec->jit->trace_capacity = new_cap;
        }
    }
    
    if (rec->jit->trace_count < rec->jit->trace_capacity) {
        rec->jit->traces[rec->jit->trace_count++] = trace;
    }
    
    rec->jit->trace_success++;
    
    return trace;
}

/**
 * @brief 中止记录
 * @param rec 记录器
 * @param reason 中止原因
 */
void ljit_rec_abort(TraceRecorder *rec, AbortReason reason) {
    rec->state = REC_ABORT;
    rec->abort_reason = reason;
    
    if (rec->trace) {
        free(rec->trace);
        rec->trace = NULL;
    }
    
    rec->jit->trace_aborts++;
    
    /* 多次中止则加入黑名单 */
    /* TODO: 实现黑名单逻辑 */
}

/* ========================================================================
 * 字节码到IR转换
 * ======================================================================== */

/**
 * @brief 获取槽位IR引用
 */
static IRRef get_slot_ref(TraceRecorder *rec, uint16_t slot) {
    /* 简化实现：为每个槽位创建一个引用 */
    return IRREF_BIAS + slot;
}

/**
 * @brief 记录移动指令
 */
void ljit_rec_mov(TraceRecorder *rec, uint16_t dst, uint16_t src) {
    IRRef src_ref = get_slot_ref(rec, src);
    ljit_ir_emit1(rec->builder, IR_MOV, rec->slot_types[src], src_ref);
    rec->slot_types[dst] = rec->slot_types[src];
}

/**
 * @brief 记录算术指令
 */
void ljit_rec_arith(TraceRecorder *rec, IROp op,
                     uint16_t dst, uint16_t src1, uint16_t src2) {
    IRRef ref1 = get_slot_ref(rec, src1);
    IRRef ref2 = get_slot_ref(rec, src2);
    
    IRType type1 = rec->slot_types[src1];
    IRType type2 = rec->slot_types[src2];
    
    /* 类型守卫 */
    if (type1 == IRT_UNKNOWN) {
        ljit_rec_guard_type(rec, src1, IRT_NUM);
        type1 = IRT_NUM;
    }
    if (type2 == IRT_UNKNOWN) {
        ljit_rec_guard_type(rec, src2, IRT_NUM);
        type2 = IRT_NUM;
    }
    
    IRType result_type = (type1 == IRT_INT && type2 == IRT_INT) ? IRT_INT : IRT_NUM;
    ljit_ir_emit2(rec->builder, op, result_type, ref1, ref2);
    rec->slot_types[dst] = result_type;
}

/**
 * @brief 记录比较指令
 */
IRRef ljit_rec_comp(TraceRecorder *rec, IROp op, uint16_t a, uint16_t b) {
    IRRef ref_a = get_slot_ref(rec, a);
    IRRef ref_b = get_slot_ref(rec, b);
    return ljit_ir_emit2(rec->builder, op, IRT_TRUE, ref_a, ref_b);
}

/**
 * @brief 记录跳转指令
 */
void ljit_rec_jump(TraceRecorder *rec, uint32_t target, IRRef cond) {
    if (cond != IRREF_NIL) {
        /* 条件跳转 */
        ljit_ir_jmp_cond(rec->builder, cond, 
                          ljit_ir_kint(rec->builder, target), true);
    } else {
        /* 无条件跳转 */
        ljit_ir_jmp(rec->builder, ljit_ir_kint(rec->builder, target));
    }
    
    rec->cur_pc = target;
}

/**
 * @brief 记录循环
 */
bool ljit_rec_loop(TraceRecorder *rec) {
    if (rec->cur_pc == rec->start_pc) {
        /* 循环闭合 */
        ljit_ir_loop(rec->builder);
        rec->in_loop = true;
        return true;
    }
    return false;
}

/**
 * @brief 记录调用
 */
void ljit_rec_call(TraceRecorder *rec, uint16_t func,
                    uint16_t nargs, uint16_t nresults) {
    if (rec->depth >= rec->max_depth) {
        ljit_rec_abort(rec, ABORT_MAX_DEPTH);
        return;
    }
    
    rec->depth++;
    
    IRRef func_ref = get_slot_ref(rec, func);
    IRRef nargs_ref = ljit_ir_kint(rec->builder, nargs);
    ljit_ir_emit2(rec->builder, IR_CALL, IRT_UNKNOWN, func_ref, nargs_ref);
    
    (void)nresults;
}

/**
 * @brief 记录返回
 */
void ljit_rec_ret(TraceRecorder *rec, uint16_t base, uint16_t nresults) {
    if (rec->depth > 0) {
        rec->depth--;
    }
    
    if (nresults == 0) {
        ljit_ir_emit0(rec->builder, IR_RET, IRT_NIL);
    } else {
        IRRef val_ref = get_slot_ref(rec, base);
        ljit_ir_emit1(rec->builder, IR_RETV, rec->slot_types[base], val_ref);
    }
}

/* ========================================================================
 * 快照管理
 * ======================================================================== */

/**
 * @brief 创建快照
 */
uint32_t ljit_rec_snapshot(TraceRecorder *rec, uint32_t pc) {
    if (rec->snap_count >= rec->snap_capacity) {
        uint32_t new_cap = rec->snap_capacity * 2;
        Snapshot *new_snaps = (Snapshot *)realloc(rec->snaps,
                                                   sizeof(Snapshot) * new_cap);
        if (!new_snaps) return rec->snap_count - 1;
        rec->snaps = new_snaps;
        rec->snap_capacity = new_cap;
    }
    
    uint32_t idx = rec->snap_count++;
    Snapshot *snap = &rec->snaps[idx];
    snap->pc = pc;
    snap->ref = rec->slot_count;
    snap->nslots = 0;
    snap->count = 0;
    snap->topslot = 0;
    
    return idx;
}

/**
 * @brief 添加快照槽位
 */
void ljit_rec_snap_slot(TraceRecorder *rec, uint32_t snap_idx,
                         uint16_t slot, IRRef ref, IRType type) {
    if (snap_idx >= rec->snap_count) return;
    
    if (rec->slot_count >= rec->slot_capacity) {
        uint32_t new_cap = rec->slot_capacity * 2;
        SnapSlot *new_slots = (SnapSlot *)realloc(rec->snap_slots,
                                                   sizeof(SnapSlot) * new_cap);
        if (!new_slots) return;
        rec->snap_slots = new_slots;
        rec->slot_capacity = new_cap;
    }
    
    SnapSlot *ss = &rec->snap_slots[rec->slot_count++];
    ss->slot = slot;
    ss->ref = ref;
    ss->type = type;
    
    Snapshot *snap = &rec->snaps[snap_idx];
    snap->nslots++;
    if (slot > snap->topslot) snap->topslot = slot;
}

/* ========================================================================
 * 类型追踪
 * ======================================================================== */

/**
 * @brief 获取槽位类型
 */
IRType ljit_rec_get_type(TraceRecorder *rec, uint16_t slot) {
    if (slot >= rec->max_slots) return IRT_UNKNOWN;
    return rec->slot_types[slot];
}

/**
 * @brief 设置槽位类型
 */
void ljit_rec_set_type(TraceRecorder *rec, uint16_t slot, IRType type) {
    if (slot < rec->max_slots) {
        rec->slot_types[slot] = type;
    }
}

/**
 * @brief 发射类型守卫
 */
IRRef ljit_rec_guard_type(TraceRecorder *rec, uint16_t slot, IRType expected) {
    IRRef ref = get_slot_ref(rec, slot);
    IRRef guard = ljit_ir_guard_type(rec->builder, ref, expected);
    
    /* 创建侧边出口快照 */
    uint32_t snap = ljit_rec_snapshot(rec, rec->cur_pc);
    ljit_ir_side_exit(rec->builder, snap);
    
    rec->slot_types[slot] = expected;
    return guard;
}

/* ========================================================================
 * 黑名单管理
 * ======================================================================== */

/* 简化实现：使用静态数组 */
#define BLACKLIST_SIZE 64

static struct {
    Proto *proto;
    uint32_t pc;
    uint8_t count;
} blacklist[BLACKLIST_SIZE];

static uint32_t blacklist_count = 0;

/**
 * @brief 将函数加入黑名单
 */
void ljit_rec_blacklist(TraceRecorder *rec, Proto *proto, uint32_t pc) {
    (void)rec;
    
    if (blacklist_count >= BLACKLIST_SIZE) return;
    
    /* 检查是否已存在 */
    for (uint32_t i = 0; i < blacklist_count; i++) {
        if (blacklist[i].proto == proto && blacklist[i].pc == pc) {
            blacklist[i].count++;
            return;
        }
    }
    
    blacklist[blacklist_count].proto = proto;
    blacklist[blacklist_count].pc = pc;
    blacklist[blacklist_count].count = 1;
    blacklist_count++;
}

/**
 * @brief 检查是否在黑名单中
 */
bool ljit_rec_is_blacklisted(TraceRecorder *rec, Proto *proto, uint32_t pc) {
    (void)rec;
    
    for (uint32_t i = 0; i < blacklist_count; i++) {
        if (blacklist[i].proto == proto && blacklist[i].pc == pc) {
            return blacklist[i].count >= 3;  /* 3次中止后加入黑名单 */
        }
    }
    return false;
}

/* ========================================================================
 * 调试
 * ======================================================================== */

static const char *abort_reasons[] = {
    "none",
    "max_instructions",
    "max_depth",
    "nyi",
    "loop_unroll",
    "blacklist",
    "type_unstable",
    "inner_loop",
    "bad_instruction"
};

/**
 * @brief 获取中止原因字符串
 */
const char *ljit_abort_reason_str(AbortReason reason) {
    if (reason < sizeof(abort_reasons) / sizeof(abort_reasons[0])) {
        return abort_reasons[reason];
    }
    return "unknown";
}

/**
 * @brief 打印记录状态
 */
void ljit_rec_dump(TraceRecorder *rec) {
    printf("=== Trace Recorder State ===\n");
    printf("State: %d\n", rec->state);
    printf("Proto: %p\n", (void *)rec->proto);
    printf("Start PC: %u\n", rec->start_pc);
    printf("Current PC: %u\n", rec->cur_pc);
    printf("BC count: %u / %u\n", rec->bc_count, rec->max_bc);
    printf("Depth: %u / %u\n", rec->depth, rec->max_depth);
    printf("Snapshots: %u\n", rec->snap_count);
    
    if (rec->state == REC_ABORT) {
        printf("Abort reason: %s\n", ljit_abort_reason_str(rec->abort_reason));
    }
    
    if (rec->builder) {
        printf("\nIR Instructions: %u\n", rec->builder->ir_cur);
        printf("Constants: %u\n", rec->builder->const_cur);
    }
}
