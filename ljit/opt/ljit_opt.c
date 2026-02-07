/*
** ljit_opt.c - IR优化器实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 优化器生命周期
 * ======================================================================== */

/**
 * @brief 初始化优化器
 * @param opt 优化器
 * @param builder IR构建器
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_init(Optimizer *opt, IRBuilder *builder) {
    memset(opt, 0, sizeof(Optimizer));
    opt->builder = builder;
    ljit_opt_config_default(&opt->config);
    return JIT_OK;
}

/**
 * @brief 销毁优化器
 * @param opt 优化器
 */
void ljit_opt_free(Optimizer *opt) {
    if (opt->ir_used) free(opt->ir_used);
    if (opt->ir_live) free(opt->ir_live);
    memset(opt, 0, sizeof(Optimizer));
}

/**
 * @brief 设置默认优化配置
 * @param config 配置结构
 */
void ljit_opt_config_default(OptConfig *config) {
    config->enable_fold = true;
    config->enable_dce = true;
    config->enable_cse = true;
    config->enable_narrow = true;
    config->enable_loop = true;
    config->enable_sink = false;  /* 默认关闭 */
    config->enable_licm = true;
    config->max_iterations = 3;
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取IR指令
 */
static IRIns *get_ir(Optimizer *opt, IRRef ref) {
    if (irref_isconst(ref)) return NULL;
    uint32_t idx = ref - IRREF_BIAS;
    if (idx >= opt->builder->ir_cur) return NULL;
    return &opt->builder->ir[idx];
}

/**
 * @brief 获取常量
 */
static IRConst *get_const(Optimizer *opt, IRRef ref) {
    if (!irref_isconst(ref)) return NULL;
    if (ref >= opt->builder->const_cur) return NULL;
    return &opt->builder->consts[ref];
}

/**
 * @brief 检查是否为纯操作（无副作用）
 */
static bool is_pure_op(IROp op) {
    switch (op) {
        case IR_NOP:
        case IR_KINT:
        case IR_KNUM:
        case IR_KPTR:
        case IR_KNIL:
        case IR_KTRUE:
        case IR_KFALSE:
        case IR_MOV:
        case IR_ADD_INT:
        case IR_SUB_INT:
        case IR_MUL_INT:
        case IR_DIV_INT:
        case IR_MOD_INT:
        case IR_NEG_INT:
        case IR_BAND:
        case IR_BOR:
        case IR_BXOR:
        case IR_BNOT:
        case IR_SHL:
        case IR_SHR:
        case IR_ADD_NUM:
        case IR_SUB_NUM:
        case IR_MUL_NUM:
        case IR_DIV_NUM:
        case IR_NEG_NUM:
        case IR_POW_NUM:
        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        case IR_GT:
        case IR_GE:
        case IR_CONV_INT_NUM:
        case IR_CONV_NUM_INT:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 检查是否为分支指令
 */
static bool is_branch_op(IROp op) {
    switch (op) {
        case IR_JMP:
        case IR_JMPT:
        case IR_JMPF:
        case IR_RET:
        case IR_RETV:
            return true;
        default:
            return false;
    }
}

/* ========================================================================
 * 优化Pass执行
 * ======================================================================== */

/**
 * @brief 执行所有启用的优化Pass
 * @param opt 优化器
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_run(Optimizer *opt) {
    for (uint32_t iter = 0; iter < opt->config.max_iterations; iter++) {
        uint32_t changes = 0;
        
        if (opt->config.enable_fold) {
            changes += ljit_opt_fold(opt);
        }
        
        if (opt->config.enable_dce) {
            changes += ljit_opt_dce(opt);
        }
        
        if (opt->config.enable_narrow) {
            changes += ljit_opt_narrow(opt);
        }
        
        if (opt->config.enable_licm) {
            changes += ljit_opt_licm(opt);
        }
        
        /* 无变化则提前退出 */
        if (changes == 0) break;
    }
    
    return JIT_OK;
}

/**
 * @brief 执行单个优化Pass
 * @param opt 优化器
 * @param pass Pass类型
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_run_pass(Optimizer *opt, OptPassType pass) {
    switch (pass) {
        case OPT_PASS_FOLD:
            ljit_opt_fold(opt);
            break;
        case OPT_PASS_DCE:
            ljit_opt_dce(opt);
            break;
        case OPT_PASS_NARROW:
            ljit_opt_narrow(opt);
            break;
        case OPT_PASS_LICM:
            ljit_opt_licm(opt);
            break;
        default:
            break;
    }
    return JIT_OK;
}

/* ========================================================================
 * 常量折叠
 * ======================================================================== */

/**
 * @brief 尝试折叠单条指令
 * @param opt 优化器
 * @param ref IR引用
 * @return 折叠后的IR引用，未折叠返回原引用
 */
IRRef ljit_opt_fold_ins(Optimizer *opt, IRRef ref) {
    IRIns *ir = get_ir(opt, ref);
    if (!ir) return ref;
    
    /* 只有两个操作数都是常量才能折叠 */
    if (!irref_isconst(ir->op1) || !irref_isconst(ir->op2)) {
        return ref;
    }
    
    IRConst *c1 = get_const(opt, ir->op1);
    IRConst *c2 = get_const(opt, ir->op2);
    if (!c1 || !c2) return ref;
    
    int64_t result_i = 0;
    double result_n = 0;
    bool folded = false;
    bool is_int_result = false;
    
    switch (ir->op) {
        case IR_ADD_INT:
            result_i = c1->i + c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_SUB_INT:
            result_i = c1->i - c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_MUL_INT:
            result_i = c1->i * c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_DIV_INT:
            if (c2->i != 0) {
                result_i = c1->i / c2->i;
                folded = true;
                is_int_result = true;
            }
            break;
        case IR_MOD_INT:
            if (c2->i != 0) {
                result_i = c1->i % c2->i;
                folded = true;
                is_int_result = true;
            }
            break;
        case IR_BAND:
            result_i = c1->i & c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_BOR:
            result_i = c1->i | c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_BXOR:
            result_i = c1->i ^ c2->i;
            folded = true;
            is_int_result = true;
            break;
        case IR_SHL:
            result_i = c1->i << (c2->i & 63);
            folded = true;
            is_int_result = true;
            break;
        case IR_SHR:
            result_i = (int64_t)((uint64_t)c1->i >> (c2->i & 63));
            folded = true;
            is_int_result = true;
            break;
        case IR_ADD_NUM:
            result_n = c1->n + c2->n;
            folded = true;
            break;
        case IR_SUB_NUM:
            result_n = c1->n - c2->n;
            folded = true;
            break;
        case IR_MUL_NUM:
            result_n = c1->n * c2->n;
            folded = true;
            break;
        case IR_DIV_NUM:
            result_n = c1->n / c2->n;
            folded = true;
            break;
        case IR_POW_NUM:
            result_n = pow(c1->n, c2->n);
            folded = true;
            break;
        default:
            break;
    }
    
    if (!folded) return ref;
    
    opt->fold_count++;
    
    if (is_int_result) {
        return ljit_ir_kint(opt->builder, result_i);
    } else {
        return ljit_ir_knum(opt->builder, result_n);
    }
}

/**
 * @brief 常量折叠Pass
 * @param opt 优化器
 * @return 折叠的指令数
 */
uint32_t ljit_opt_fold(Optimizer *opt) {
    uint32_t count = 0;
    IRBuilder *builder = opt->builder;
    
    for (uint32_t i = 0; i < builder->ir_cur; i++) {
        IRRef ref = IRREF_BIAS + i;
        IRRef folded = ljit_opt_fold_ins(opt, ref);
        
        if (folded != ref) {
            /* 替换为NOP */
            IRIns *ir = &builder->ir[i];
            ir->op = IR_NOP;
            ir->op1 = folded;  /* 保存折叠结果 */
            ir->op2 = IRREF_NIL;
            count++;
        }
    }
    
    return count;
}

/* ========================================================================
 * 死代码消除
 * ======================================================================== */

/**
 * @brief 标记活跃指令
 * @param opt 优化器
 */
void ljit_opt_mark_live(Optimizer *opt) {
    IRBuilder *builder = opt->builder;
    uint32_t size = builder->ir_cur;
    
    /* 分配/重置标记数组 */
    if (!opt->ir_live || size > builder->ir_max) {
        if (opt->ir_live) free(opt->ir_live);
        opt->ir_live = (uint8_t *)calloc(builder->ir_max, 1);
    } else {
        memset(opt->ir_live, 0, size);
    }
    
    /* 从根开始标记 (返回、存储、守卫等有副作用的指令) */
    for (uint32_t i = 0; i < size; i++) {
        IRIns *ir = &builder->ir[i];
        
        /* 有副作用的指令必须保留 */
        if (!is_pure_op(ir->op) || is_branch_op(ir->op)) {
            opt->ir_live[i] = 1;
        }
    }
    
    /* 反向传播活跃性 */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = (int32_t)size - 1; i >= 0; i--) {
            if (!opt->ir_live[i]) continue;
            
            IRIns *ir = &builder->ir[i];
            
            /* 标记操作数为活跃 */
            if (irref_isvar(ir->op1)) {
                uint32_t idx = ir->op1 - IRREF_BIAS;
                if (idx < size && !opt->ir_live[idx]) {
                    opt->ir_live[idx] = 1;
                    changed = true;
                }
            }
            
            if (irref_isvar(ir->op2)) {
                uint32_t idx = ir->op2 - IRREF_BIAS;
                if (idx < size && !opt->ir_live[idx]) {
                    opt->ir_live[idx] = 1;
                    changed = true;
                }
            }
        }
    }
}

/**
 * @brief 死代码消除Pass
 * @param opt 优化器
 * @return 消除的指令数
 */
uint32_t ljit_opt_dce(Optimizer *opt) {
    ljit_opt_mark_live(opt);
    
    IRBuilder *builder = opt->builder;
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < builder->ir_cur; i++) {
        if (!opt->ir_live[i]) {
            IRIns *ir = &builder->ir[i];
            if (ir->op != IR_NOP) {
                ir->op = IR_NOP;
                ir->op1 = IRREF_NIL;
                ir->op2 = IRREF_NIL;
                count++;
                opt->dce_count++;
            }
        }
    }
    
    return count;
}

/* ========================================================================
 * 循环不变代码外提
 * ======================================================================== */

/**
 * @brief 检查指令是否循环不变
 */
static bool is_loop_invariant(Optimizer *opt, IRRef ref, uint32_t loop_start) {
    if (irref_isconst(ref)) return true;
    
    uint32_t idx = ref - IRREF_BIAS;
    if (idx < loop_start) return true;  /* 在循环之前定义 */
    
    IRIns *ir = get_ir(opt, ref);
    if (!ir) return false;
    
    if (!is_pure_op(ir->op)) return false;
    
    return is_loop_invariant(opt, ir->op1, loop_start) &&
           is_loop_invariant(opt, ir->op2, loop_start);
}

/**
 * @brief 循环不变代码外提
 * @param opt 优化器
 * @return 外提的指令数
 */
uint32_t ljit_opt_licm(Optimizer *opt) {
    IRBuilder *builder = opt->builder;
    uint32_t count = 0;
    
    /* 找到循环开始位置 */
    if (builder->loop_start == 0) return 0;
    
    uint32_t loop_start = builder->loop_start;
    
    /* 扫描循环内的指令 */
    for (uint32_t i = loop_start; i < builder->ir_cur; i++) {
        IRIns *ir = &builder->ir[i];
        
        if (!is_pure_op(ir->op)) continue;
        if (ir->op == IR_NOP) continue;
        
        if (is_loop_invariant(opt, IRREF_BIAS + i, loop_start)) {
            /* 标记为可外提 - 实际外提需要更复杂的处理 */
            /* 这里简化为只标记 */
            count++;
        }
    }
    
    return count;
}

/**
 * @brief 循环展开
 * @param opt 优化器
 * @param factor 展开因子
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_unroll(Optimizer *opt, uint32_t factor) {
    /* TODO: 实现循环展开 */
    (void)opt;
    (void)factor;
    return JIT_OK;
}

/* ========================================================================
 * 类型窄化
 * ======================================================================== */

/**
 * @brief 类型窄化Pass
 * @param opt 优化器
 * @return 窄化的指令数
 */
uint32_t ljit_opt_narrow(Optimizer *opt) {
    IRBuilder *builder = opt->builder;
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < builder->ir_cur; i++) {
        IRIns *ir = &builder->ir[i];
        
        /* 查找可以窄化为整数运算的浮点运算 */
        if (ir->op == IR_ADD_NUM || ir->op == IR_SUB_NUM || ir->op == IR_MUL_NUM) {
            /* 检查操作数是否实际为整数 */
            bool can_narrow = true;
            
            if (irref_isconst(ir->op1)) {
                IRConst *c = get_const(opt, ir->op1);
                if (c && c->n != (double)(int64_t)c->n) {
                    can_narrow = false;
                }
            } else {
                IRIns *op1_ir = get_ir(opt, ir->op1);
                if (op1_ir && op1_ir->type != IRT_INT) {
                    can_narrow = false;
                }
            }
            
            if (can_narrow && irref_isconst(ir->op2)) {
                IRConst *c = get_const(opt, ir->op2);
                if (c && c->n != (double)(int64_t)c->n) {
                    can_narrow = false;
                }
            } else if (can_narrow) {
                IRIns *op2_ir = get_ir(opt, ir->op2);
                if (op2_ir && op2_ir->type != IRT_INT) {
                    can_narrow = false;
                }
            }
            
            if (can_narrow) {
                /* 窄化为整数运算 */
                switch (ir->op) {
                    case IR_ADD_NUM: ir->op = IR_ADD_INT; break;
                    case IR_SUB_NUM: ir->op = IR_SUB_INT; break;
                    case IR_MUL_NUM: ir->op = IR_MUL_INT; break;
                    default: break;
                }
                ir->type = IRT_INT;
                count++;
            }
        }
    }
    
    return count;
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印优化统计
 * @param opt 优化器
 */
void ljit_opt_dump_stats(Optimizer *opt) {
    printf("=== Optimization Stats ===\n");
    printf("Constant folding: %u\n", opt->fold_count);
    printf("Dead code elimination: %u\n", opt->dce_count);
    printf("CSE: %u\n", opt->cse_count);
}
