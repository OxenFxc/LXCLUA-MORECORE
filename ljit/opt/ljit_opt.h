/*
** ljit_opt.h - IR优化器接口
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_opt_h
#define ljit_opt_h

#include "../ljit_types.h"
#include "../ir/ljit_ir.h"

/* ========================================================================
 * 优化Pass类型
 * ======================================================================== */

/**
 * @brief 优化Pass类型
 */
typedef enum {
    OPT_PASS_FOLD,           /**< 常量折叠 */
    OPT_PASS_DCE,            /**< 死代码消除 */
    OPT_PASS_CSE,            /**< 公共子表达式消除 */
    OPT_PASS_NARROW,         /**< 类型窄化 */
    OPT_PASS_LOOP,           /**< 循环优化 */
    OPT_PASS_SINK,           /**< 代码下沉 */
    OPT_PASS_LICM,           /**< 循环不变代码外提 */
    OPT_PASS__MAX
} OptPassType;

/**
 * @brief 优化配置
 */
typedef struct OptConfig {
    bool enable_fold;        /**< 启用常量折叠 */
    bool enable_dce;         /**< 启用死代码消除 */
    bool enable_cse;         /**< 启用CSE */
    bool enable_narrow;      /**< 启用类型窄化 */
    bool enable_loop;        /**< 启用循环优化 */
    bool enable_sink;        /**< 启用代码下沉 */
    bool enable_licm;        /**< 启用LICM */
    uint32_t max_iterations; /**< 最大优化迭代次数 */
} OptConfig;

/**
 * @brief 优化器状态
 */
typedef struct Optimizer {
    IRBuilder *builder;      /**< IR构建器 */
    OptConfig config;        /**< 优化配置 */
    
    /* 分析结果 */
    uint8_t *ir_used;        /**< IR使用标记 */
    uint8_t *ir_live;        /**< IR活跃标记 */
    
    /* 统计 */
    uint32_t fold_count;
    uint32_t dce_count;
    uint32_t cse_count;
} Optimizer;

/* ========================================================================
 * 优化器生命周期
 * ======================================================================== */

/**
 * @brief 初始化优化器
 * @param opt 优化器
 * @param builder IR构建器
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_init(Optimizer *opt, IRBuilder *builder);

/**
 * @brief 销毁优化器
 * @param opt 优化器
 */
void ljit_opt_free(Optimizer *opt);

/**
 * @brief 设置默认优化配置
 * @param config 配置结构
 */
void ljit_opt_config_default(OptConfig *config);

/* ========================================================================
 * 优化Pass执行
 * ======================================================================== */

/**
 * @brief 执行所有启用的优化Pass
 * @param opt 优化器
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_run(Optimizer *opt);

/**
 * @brief 执行单个优化Pass
 * @param opt 优化器
 * @param pass Pass类型
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_run_pass(Optimizer *opt, OptPassType pass);

/* ========================================================================
 * 常量折叠
 * ======================================================================== */

/**
 * @brief 常量折叠Pass
 * @param opt 优化器
 * @return 折叠的指令数
 */
uint32_t ljit_opt_fold(Optimizer *opt);

/**
 * @brief 尝试折叠单条指令
 * @param opt 优化器
 * @param ref IR引用
 * @return 折叠后的IR引用，未折叠返回原引用
 */
IRRef ljit_opt_fold_ins(Optimizer *opt, IRRef ref);

/* ========================================================================
 * 死代码消除
 * ======================================================================== */

/**
 * @brief 死代码消除Pass
 * @param opt 优化器
 * @return 消除的指令数
 */
uint32_t ljit_opt_dce(Optimizer *opt);

/**
 * @brief 标记活跃指令
 * @param opt 优化器
 */
void ljit_opt_mark_live(Optimizer *opt);

/* ========================================================================
 * 循环优化
 * ======================================================================== */

/**
 * @brief 循环不变代码外提
 * @param opt 优化器
 * @return 外提的指令数
 */
uint32_t ljit_opt_licm(Optimizer *opt);

/**
 * @brief 循环展开 (受限展开)
 * @param opt 优化器
 * @param factor 展开因子
 * @return 成功返回JIT_OK
 */
JitError ljit_opt_unroll(Optimizer *opt, uint32_t factor);

/* ========================================================================
 * 类型窄化
 * ======================================================================== */

/**
 * @brief 类型窄化Pass
 * @param opt 优化器
 * @return 窄化的指令数
 */
uint32_t ljit_opt_narrow(Optimizer *opt);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印优化统计
 * @param opt 优化器
 */
void ljit_opt_dump_stats(Optimizer *opt);

#endif /* ljit_opt_h */
