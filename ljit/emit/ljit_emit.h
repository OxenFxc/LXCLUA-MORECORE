/*
** ljit_emit.h - 机器码发射器接口
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_emit_h
#define ljit_emit_h

#include "../ljit_types.h"
#include "../ir/ljit_ir.h"

/* ========================================================================
 * 目标架构
 * ======================================================================== */

typedef enum {
    ARCH_X64 = 0,
    ARCH_ARM64,
    ARCH_UNKNOWN
} TargetArch;

/* 自动检测当前架构 */
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    #define LJIT_ARCH ARCH_X64
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define LJIT_ARCH ARCH_ARM64
#else
    #define LJIT_ARCH ARCH_UNKNOWN
#endif

/* ========================================================================
 * x64寄存器定义
 * ======================================================================== */

typedef enum {
    REG_RAX = 0,
    REG_RCX,
    REG_RDX,
    REG_RBX,
    REG_RSP,
    REG_RBP,
    REG_RSI,
    REG_RDI,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_COUNT,
    REG_NONE = 0xFF
} X64Reg;

/* XMM寄存器 */
typedef enum {
    XMM0 = 0, XMM1, XMM2, XMM3,
    XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11,
    XMM12, XMM13, XMM14, XMM15,
    XMM_COUNT,
    XMM_NONE = 0xFF
} X64XmmReg;

/* ========================================================================
 * 寄存器分配器
 * ======================================================================== */

/**
 * @brief 寄存器分配信息
 */
typedef struct RegAlloc {
    uint8_t gpr_map[REG_COUNT];      /**< GPR -> IR引用映射 */
    uint8_t xmm_map[XMM_COUNT];      /**< XMM -> IR引用映射 */
    uint16_t ir_reg[IR_MAX_SIZE];    /**< IR引用 -> 寄存器映射 */
    uint32_t gpr_free;               /**< 空闲GPR位图 */
    uint32_t xmm_free;               /**< 空闲XMM位图 */
    uint32_t gpr_saved;              /**< 需保存的GPR位图 */
} RegAlloc;

/* ========================================================================
 * 代码缓冲区
 * ======================================================================== */

/**
 * @brief 机器码缓冲区
 */
typedef struct CodeBuffer {
    uint8_t *code;           /**< 代码起始地址 */
    uint8_t *cur;            /**< 当前写入位置 */
    uint8_t *end;            /**< 缓冲区结束 */
    size_t size;             /**< 缓冲区大小 */
    
    /* 标签和跳转补丁 */
    uint32_t *labels;        /**< 标签位置数组 */
    uint32_t label_count;
    
    struct {
        uint32_t code_offset;
        uint32_t label_idx;
        int8_t offset_size;  /**< 1, 2, 4字节 */
    } *patches;
    uint32_t patch_count;
    uint32_t patch_capacity;
    
} CodeBuffer;

/* ========================================================================
 * 发射器状态
 * ======================================================================== */

/**
 * @brief 代码发射器
 */
typedef struct Emitter {
    IRBuilder *builder;      /**< IR构建器 */
    CodeBuffer code;         /**< 代码缓冲区 */
    RegAlloc regs;           /**< 寄存器分配 */
    TargetArch arch;         /**< 目标架构 */
    
    /* Lua状态指针 */
    void *L;                 /**< lua_State* */
    
    /* 栈帧信息 */
    uint32_t frame_size;     /**< 栈帧大小 */
    uint32_t spill_offset;   /**< 溢出区起始偏移 */
    
    /* 侧边出口表 */
    uint32_t *exit_stubs;    /**< 出口存根偏移 */
    uint32_t exit_count;
    
} Emitter;

/* ========================================================================
 * 发射器生命周期
 * ======================================================================== */

/**
 * @brief 初始化发射器
 * @param emit 发射器
 * @param builder IR构建器
 * @param code_mem 可执行内存
 * @param code_size 内存大小
 * @return 成功返回JIT_OK
 */
JitError ljit_emit_init(Emitter *emit, IRBuilder *builder, 
                         void *code_mem, size_t code_size);

/**
 * @brief 销毁发射器
 * @param emit 发射器
 */
void ljit_emit_free(Emitter *emit);

/**
 * @brief 重置发射器
 * @param emit 发射器
 */
void ljit_emit_reset(Emitter *emit);

/* ========================================================================
 * 寄存器分配
 * ======================================================================== */

/**
 * @brief 初始化寄存器分配器
 * @param emit 发射器
 */
void ljit_emit_reg_init(Emitter *emit);

/**
 * @brief 分配GPR
 * @param emit 发射器
 * @param ir_ref 关联的IR引用
 * @return 分配的寄存器
 */
X64Reg ljit_emit_alloc_gpr(Emitter *emit, IRRef ir_ref);

/**
 * @brief 分配XMM
 * @param emit 发射器
 * @param ir_ref 关联的IR引用
 * @return 分配的寄存器
 */
X64XmmReg ljit_emit_alloc_xmm(Emitter *emit, IRRef ir_ref);

/**
 * @brief 释放寄存器
 * @param emit 发射器
 * @param ir_ref IR引用
 */
void ljit_emit_free_reg(Emitter *emit, IRRef ir_ref);

/**
 * @brief 溢出寄存器到栈
 * @param emit 发射器
 * @param reg 寄存器
 */
void ljit_emit_spill(Emitter *emit, X64Reg reg);

/**
 * @brief 从栈恢复寄存器
 * @param emit 发射器
 * @param reg 寄存器
 * @param ir_ref IR引用
 */
void ljit_emit_reload(Emitter *emit, X64Reg reg, IRRef ir_ref);

/* ========================================================================
 * 代码生成主入口
 * ======================================================================== */

/**
 * @brief 生成整个Trace的机器码
 * @param emit 发射器
 * @param trace Trace结构
 * @return 成功返回JIT_OK
 */
JitError ljit_emit_trace(Emitter *emit, Trace *trace);

/**
 * @brief 生成单条IR指令
 * @param emit 发射器
 * @param ir_ref IR引用
 * @return 成功返回JIT_OK
 */
JitError ljit_emit_ir(Emitter *emit, IRRef ir_ref);

/* ========================================================================
 * 序言和尾声
 * ======================================================================== */

/**
 * @brief 生成函数序言
 * @param emit 发射器
 */
void ljit_emit_prologue(Emitter *emit);

/**
 * @brief 生成函数尾声
 * @param emit 发射器
 */
void ljit_emit_epilogue(Emitter *emit);

/* ========================================================================
 * 侧边出口
 * ======================================================================== */

/**
 * @brief 生成侧边出口存根
 * @param emit 发射器
 * @param exit_idx 出口索引
 * @param snap_idx 快照索引
 */
void ljit_emit_exit_stub(Emitter *emit, uint32_t exit_idx, uint32_t snap_idx);

/**
 * @brief 发射侧边出口跳转
 * @param emit 发射器
 * @param exit_idx 出口索引
 */
void ljit_emit_exit_jump(Emitter *emit, uint32_t exit_idx);

/* ========================================================================
 * 低级代码发射 (x64)
 * ======================================================================== */

/* 指令编码辅助 */
void emit_byte(CodeBuffer *cb, uint8_t b);
void emit_word(CodeBuffer *cb, uint16_t w);
void emit_dword(CodeBuffer *cb, uint32_t d);
void emit_qword(CodeBuffer *cb, uint64_t q);
void emit_bytes(CodeBuffer *cb, const uint8_t *data, size_t len);

/* REX前缀 */
void emit_rex(CodeBuffer *cb, bool w, X64Reg r, X64Reg x, X64Reg b);

/* ModR/M编码 */
void emit_modrm(CodeBuffer *cb, uint8_t mod, uint8_t reg, uint8_t rm);
void emit_sib(CodeBuffer *cb, uint8_t scale, uint8_t index, uint8_t base);

/* 常用指令 */
void emit_mov_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_mov_ri(Emitter *emit, X64Reg dst, int64_t imm);
void emit_mov_rm(Emitter *emit, X64Reg dst, X64Reg base, int32_t offset);
void emit_mov_mr(Emitter *emit, X64Reg base, int32_t offset, X64Reg src);

void emit_add_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_add_ri(Emitter *emit, X64Reg dst, int32_t imm);
void emit_sub_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_sub_ri(Emitter *emit, X64Reg dst, int32_t imm);
void emit_imul_rr(Emitter *emit, X64Reg dst, X64Reg src);

void emit_and_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_or_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_xor_rr(Emitter *emit, X64Reg dst, X64Reg src);
void emit_shl_ri(Emitter *emit, X64Reg dst, uint8_t imm);
void emit_shr_ri(Emitter *emit, X64Reg dst, uint8_t imm);

void emit_cmp_rr(Emitter *emit, X64Reg a, X64Reg b);
void emit_cmp_ri(Emitter *emit, X64Reg a, int32_t imm);
void emit_test_rr(Emitter *emit, X64Reg a, X64Reg b);

void emit_jmp_rel32(Emitter *emit, int32_t offset);
void emit_jcc_rel32(Emitter *emit, uint8_t cc, int32_t offset);
void emit_call_rel32(Emitter *emit, int32_t offset);
void emit_ret(Emitter *emit);

void emit_push(Emitter *emit, X64Reg reg);
void emit_pop(Emitter *emit, X64Reg reg);

/* XMM指令 */
void emit_movsd_xr(Emitter *emit, X64XmmReg dst, X64Reg base, int32_t offset);
void emit_movsd_rx(Emitter *emit, X64Reg base, int32_t offset, X64XmmReg src);
void emit_addsd(Emitter *emit, X64XmmReg dst, X64XmmReg src);
void emit_subsd(Emitter *emit, X64XmmReg dst, X64XmmReg src);
void emit_mulsd(Emitter *emit, X64XmmReg dst, X64XmmReg src);
void emit_divsd(Emitter *emit, X64XmmReg dst, X64XmmReg src);

/* ========================================================================
 * 标签和跳转补丁
 * ======================================================================== */

/**
 * @brief 创建标签
 * @param emit 发射器
 * @return 标签索引
 */
uint32_t ljit_emit_label(Emitter *emit);

/**
 * @brief 绑定标签到当前位置
 * @param emit 发射器
 * @param label 标签索引
 */
void ljit_emit_bind_label(Emitter *emit, uint32_t label);

/**
 * @brief 应用所有跳转补丁
 * @param emit 发射器
 */
void ljit_emit_apply_patches(Emitter *emit);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 反汇编生成的代码
 * @param emit 发射器
 */
void ljit_emit_disasm(Emitter *emit);

#endif /* ljit_emit_h */
