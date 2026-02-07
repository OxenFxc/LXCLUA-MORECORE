/*
** ljit_emit_x64.c - x64机器码发射器实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_emit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 条件码定义
 * ======================================================================== */

#define CC_O    0x0
#define CC_NO   0x1
#define CC_B    0x2
#define CC_AE   0x3
#define CC_E    0x4
#define CC_NE   0x5
#define CC_BE   0x6
#define CC_A    0x7
#define CC_S    0x8
#define CC_NS   0x9
#define CC_P    0xA
#define CC_NP   0xB
#define CC_L    0xC
#define CC_GE   0xD
#define CC_LE   0xE
#define CC_G    0xF

/* 可用于分配的GPR (排除RSP, RBP) */
#define ALLOCABLE_GPRS ((1 << REG_RAX) | (1 << REG_RCX) | (1 << REG_RDX) | \
                        (1 << REG_RBX) | (1 << REG_RSI) | (1 << REG_RDI) | \
                        (1 << REG_R8)  | (1 << REG_R9)  | (1 << REG_R10) | \
                        (1 << REG_R11) | (1 << REG_R12) | (1 << REG_R13) | \
                        (1 << REG_R14) | (1 << REG_R15))

/* 调用约定保存寄存器 */
#ifdef _WIN64
#define CALLEE_SAVED ((1 << REG_RBX) | (1 << REG_RSI) | (1 << REG_RDI) | \
                      (1 << REG_R12) | (1 << REG_R13) | (1 << REG_R14) | \
                      (1 << REG_R15))
#else
#define CALLEE_SAVED ((1 << REG_RBX) | (1 << REG_R12) | (1 << REG_R13) | \
                      (1 << REG_R14) | (1 << REG_R15))
#endif

/* ========================================================================
 * 基础字节发射
 * ======================================================================== */

void emit_byte(CodeBuffer *cb, uint8_t b) {
    if (cb->cur < cb->end) {
        *cb->cur++ = b;
    }
}

void emit_word(CodeBuffer *cb, uint16_t w) {
    emit_byte(cb, w & 0xFF);
    emit_byte(cb, (w >> 8) & 0xFF);
}

void emit_dword(CodeBuffer *cb, uint32_t d) {
    emit_byte(cb, d & 0xFF);
    emit_byte(cb, (d >> 8) & 0xFF);
    emit_byte(cb, (d >> 16) & 0xFF);
    emit_byte(cb, (d >> 24) & 0xFF);
}

void emit_qword(CodeBuffer *cb, uint64_t q) {
    emit_dword(cb, (uint32_t)q);
    emit_dword(cb, (uint32_t)(q >> 32));
}

void emit_bytes(CodeBuffer *cb, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len && cb->cur < cb->end; i++) {
        *cb->cur++ = data[i];
    }
}

/* ========================================================================
 * x64编码辅助
 * ======================================================================== */

void emit_rex(CodeBuffer *cb, bool w, X64Reg r, X64Reg x, X64Reg b) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r >= REG_R8) rex |= 0x04;
    if (x >= REG_R8) rex |= 0x02;
    if (b >= REG_R8) rex |= 0x01;
    
    if (rex != 0x40 || w) {
        emit_byte(cb, rex);
    }
}

void emit_modrm(CodeBuffer *cb, uint8_t mod, uint8_t reg, uint8_t rm) {
    emit_byte(cb, (mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

void emit_sib(CodeBuffer *cb, uint8_t scale, uint8_t index, uint8_t base) {
    emit_byte(cb, (scale << 6) | ((index & 7) << 3) | (base & 7));
}

/* ========================================================================
 * 寄存器间移动
 * ======================================================================== */

void emit_mov_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, dst, 0, src);
    emit_byte(cb, 0x89);  /* MOV r/m64, r64 */
    emit_modrm(cb, 3, src, dst);
}

void emit_mov_ri(Emitter *emit, X64Reg dst, int64_t imm) {
    CodeBuffer *cb = &emit->code;
    
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        /* 32位立即数，符号扩展 */
        emit_rex(cb, true, 0, 0, dst);
        emit_byte(cb, 0xC7);
        emit_modrm(cb, 3, 0, dst);
        emit_dword(cb, (uint32_t)imm);
    } else {
        /* 64位立即数 */
        emit_rex(cb, true, 0, 0, dst);
        emit_byte(cb, 0xB8 + (dst & 7));
        emit_qword(cb, (uint64_t)imm);
    }
}

void emit_mov_rm(Emitter *emit, X64Reg dst, X64Reg base, int32_t offset) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, dst, 0, base);
    emit_byte(cb, 0x8B);  /* MOV r64, r/m64 */
    
    if (offset == 0 && (base & 7) != REG_RBP) {
        emit_modrm(cb, 0, dst, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(cb, 1, dst, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
        emit_byte(cb, (uint8_t)offset);
    } else {
        emit_modrm(cb, 2, dst, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
        emit_dword(cb, (uint32_t)offset);
    }
}

void emit_mov_mr(Emitter *emit, X64Reg base, int32_t offset, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, base);
    emit_byte(cb, 0x89);  /* MOV r/m64, r64 */
    
    if (offset == 0 && (base & 7) != REG_RBP) {
        emit_modrm(cb, 0, src, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(cb, 1, src, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
        emit_byte(cb, (uint8_t)offset);
    } else {
        emit_modrm(cb, 2, src, base);
        if ((base & 7) == REG_RSP) {
            emit_sib(cb, 0, REG_RSP, REG_RSP);
        }
        emit_dword(cb, (uint32_t)offset);
    }
}

/* ========================================================================
 * 算术指令
 * ======================================================================== */

void emit_add_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, dst);
    emit_byte(cb, 0x01);
    emit_modrm(cb, 3, src, dst);
}

void emit_add_ri(Emitter *emit, X64Reg dst, int32_t imm) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, 0, 0, dst);
    
    if (imm >= -128 && imm <= 127) {
        emit_byte(cb, 0x83);
        emit_modrm(cb, 3, 0, dst);
        emit_byte(cb, (uint8_t)imm);
    } else {
        emit_byte(cb, 0x81);
        emit_modrm(cb, 3, 0, dst);
        emit_dword(cb, (uint32_t)imm);
    }
}

void emit_sub_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, dst);
    emit_byte(cb, 0x29);
    emit_modrm(cb, 3, src, dst);
}

void emit_sub_ri(Emitter *emit, X64Reg dst, int32_t imm) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, 0, 0, dst);
    
    if (imm >= -128 && imm <= 127) {
        emit_byte(cb, 0x83);
        emit_modrm(cb, 3, 5, dst);
        emit_byte(cb, (uint8_t)imm);
    } else {
        emit_byte(cb, 0x81);
        emit_modrm(cb, 3, 5, dst);
        emit_dword(cb, (uint32_t)imm);
    }
}

void emit_imul_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, dst, 0, src);
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0xAF);
    emit_modrm(cb, 3, dst, src);
}

/* ========================================================================
 * 位运算
 * ======================================================================== */

void emit_and_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, dst);
    emit_byte(cb, 0x21);
    emit_modrm(cb, 3, src, dst);
}

void emit_or_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, dst);
    emit_byte(cb, 0x09);
    emit_modrm(cb, 3, src, dst);
}

void emit_xor_rr(Emitter *emit, X64Reg dst, X64Reg src) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, src, 0, dst);
    emit_byte(cb, 0x31);
    emit_modrm(cb, 3, src, dst);
}

void emit_shl_ri(Emitter *emit, X64Reg dst, uint8_t imm) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, 0, 0, dst);
    if (imm == 1) {
        emit_byte(cb, 0xD1);
        emit_modrm(cb, 3, 4, dst);
    } else {
        emit_byte(cb, 0xC1);
        emit_modrm(cb, 3, 4, dst);
        emit_byte(cb, imm);
    }
}

void emit_shr_ri(Emitter *emit, X64Reg dst, uint8_t imm) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, 0, 0, dst);
    if (imm == 1) {
        emit_byte(cb, 0xD1);
        emit_modrm(cb, 3, 5, dst);
    } else {
        emit_byte(cb, 0xC1);
        emit_modrm(cb, 3, 5, dst);
        emit_byte(cb, imm);
    }
}

/* ========================================================================
 * 比较
 * ======================================================================== */

void emit_cmp_rr(Emitter *emit, X64Reg a, X64Reg b) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, b, 0, a);
    emit_byte(cb, 0x39);
    emit_modrm(cb, 3, b, a);
}

void emit_cmp_ri(Emitter *emit, X64Reg a, int32_t imm) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, 0, 0, a);
    
    if (imm >= -128 && imm <= 127) {
        emit_byte(cb, 0x83);
        emit_modrm(cb, 3, 7, a);
        emit_byte(cb, (uint8_t)imm);
    } else {
        emit_byte(cb, 0x81);
        emit_modrm(cb, 3, 7, a);
        emit_dword(cb, (uint32_t)imm);
    }
}

void emit_test_rr(Emitter *emit, X64Reg a, X64Reg b) {
    CodeBuffer *cb = &emit->code;
    emit_rex(cb, true, b, 0, a);
    emit_byte(cb, 0x85);
    emit_modrm(cb, 3, b, a);
}

/* ========================================================================
 * 跳转
 * ======================================================================== */

void emit_jmp_rel32(Emitter *emit, int32_t offset) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xE9);
    emit_dword(cb, (uint32_t)offset);
}

void emit_jcc_rel32(Emitter *emit, uint8_t cc, int32_t offset) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x80 + cc);
    emit_dword(cb, (uint32_t)offset);
}

void emit_call_rel32(Emitter *emit, int32_t offset) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xE8);
    emit_dword(cb, (uint32_t)offset);
}

void emit_ret(Emitter *emit) {
    emit_byte(&emit->code, 0xC3);
}

/* ========================================================================
 * 栈操作
 * ======================================================================== */

void emit_push(Emitter *emit, X64Reg reg) {
    CodeBuffer *cb = &emit->code;
    if (reg >= REG_R8) {
        emit_byte(cb, 0x41);
    }
    emit_byte(cb, 0x50 + (reg & 7));
}

void emit_pop(Emitter *emit, X64Reg reg) {
    CodeBuffer *cb = &emit->code;
    if (reg >= REG_R8) {
        emit_byte(cb, 0x41);
    }
    emit_byte(cb, 0x58 + (reg & 7));
}

/* ========================================================================
 * XMM指令
 * ======================================================================== */

void emit_movsd_xr(Emitter *emit, X64XmmReg dst, X64Reg base, int32_t offset) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (dst >= XMM8 || base >= REG_R8) {
        uint8_t rex = 0x40;
        if (dst >= XMM8) rex |= 0x04;
        if (base >= REG_R8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x10);
    
    if (offset == 0 && (base & 7) != REG_RBP) {
        emit_modrm(cb, 0, dst & 7, base & 7);
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(cb, 1, dst & 7, base & 7);
        emit_byte(cb, (uint8_t)offset);
    } else {
        emit_modrm(cb, 2, dst & 7, base & 7);
        emit_dword(cb, (uint32_t)offset);
    }
}

void emit_movsd_rx(Emitter *emit, X64Reg base, int32_t offset, X64XmmReg src) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (src >= XMM8 || base >= REG_R8) {
        uint8_t rex = 0x40;
        if (src >= XMM8) rex |= 0x04;
        if (base >= REG_R8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x11);
    
    if (offset == 0 && (base & 7) != REG_RBP) {
        emit_modrm(cb, 0, src & 7, base & 7);
    } else if (offset >= -128 && offset <= 127) {
        emit_modrm(cb, 1, src & 7, base & 7);
        emit_byte(cb, (uint8_t)offset);
    } else {
        emit_modrm(cb, 2, src & 7, base & 7);
        emit_dword(cb, (uint32_t)offset);
    }
}

void emit_addsd(Emitter *emit, X64XmmReg dst, X64XmmReg src) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (dst >= XMM8 || src >= XMM8) {
        uint8_t rex = 0x40;
        if (dst >= XMM8) rex |= 0x04;
        if (src >= XMM8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x58);
    emit_modrm(cb, 3, dst & 7, src & 7);
}

void emit_subsd(Emitter *emit, X64XmmReg dst, X64XmmReg src) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (dst >= XMM8 || src >= XMM8) {
        uint8_t rex = 0x40;
        if (dst >= XMM8) rex |= 0x04;
        if (src >= XMM8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x5C);
    emit_modrm(cb, 3, dst & 7, src & 7);
}

void emit_mulsd(Emitter *emit, X64XmmReg dst, X64XmmReg src) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (dst >= XMM8 || src >= XMM8) {
        uint8_t rex = 0x40;
        if (dst >= XMM8) rex |= 0x04;
        if (src >= XMM8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x59);
    emit_modrm(cb, 3, dst & 7, src & 7);
}

void emit_divsd(Emitter *emit, X64XmmReg dst, X64XmmReg src) {
    CodeBuffer *cb = &emit->code;
    emit_byte(cb, 0xF2);
    if (dst >= XMM8 || src >= XMM8) {
        uint8_t rex = 0x40;
        if (dst >= XMM8) rex |= 0x04;
        if (src >= XMM8) rex |= 0x01;
        emit_byte(cb, rex);
    }
    emit_byte(cb, 0x0F);
    emit_byte(cb, 0x5E);
    emit_modrm(cb, 3, dst & 7, src & 7);
}

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
                         void *code_mem, size_t code_size) {
    memset(emit, 0, sizeof(Emitter));
    emit->builder = builder;
    emit->arch = LJIT_ARCH;
    
    emit->code.code = (uint8_t *)code_mem;
    emit->code.cur = emit->code.code;
    emit->code.end = emit->code.code + code_size;
    emit->code.size = code_size;
    
    emit->code.patches = (void *)malloc(sizeof(*emit->code.patches) * 64);
    if (!emit->code.patches) return JIT_ERR_MEMORY;
    emit->code.patch_capacity = 64;
    
    emit->code.labels = (uint32_t *)malloc(sizeof(uint32_t) * 64);
    if (!emit->code.labels) {
        free(emit->code.patches);
        return JIT_ERR_MEMORY;
    }
    
    ljit_emit_reg_init(emit);
    
    return JIT_OK;
}

/**
 * @brief 销毁发射器
 * @param emit 发射器
 */
void ljit_emit_free(Emitter *emit) {
    if (emit->code.patches) free(emit->code.patches);
    if (emit->code.labels) free(emit->code.labels);
    if (emit->exit_stubs) free(emit->exit_stubs);
    memset(emit, 0, sizeof(Emitter));
}

/**
 * @brief 重置发射器
 * @param emit 发射器
 */
void ljit_emit_reset(Emitter *emit) {
    emit->code.cur = emit->code.code;
    emit->code.label_count = 0;
    emit->code.patch_count = 0;
    emit->exit_count = 0;
    ljit_emit_reg_init(emit);
}

/* ========================================================================
 * 寄存器分配
 * ======================================================================== */

void ljit_emit_reg_init(Emitter *emit) {
    memset(&emit->regs, 0, sizeof(RegAlloc));
    emit->regs.gpr_free = ALLOCABLE_GPRS;
    emit->regs.xmm_free = 0xFFFF;  /* 所有XMM可用 */
    
    for (int i = 0; i < REG_COUNT; i++) {
        emit->regs.gpr_map[i] = 0xFF;
    }
    for (int i = 0; i < XMM_COUNT; i++) {
        emit->regs.xmm_map[i] = 0xFF;
    }
}

X64Reg ljit_emit_alloc_gpr(Emitter *emit, IRRef ir_ref) {
    /* 找到第一个空闲寄存器 */
    uint32_t free = emit->regs.gpr_free;
    if (free == 0) {
        /* 需要溢出 - 简化实现选择RAX */
        ljit_emit_spill(emit, REG_RAX);
        free = (1 << REG_RAX);
    }
    
    /* 选择第一个空闲位 */
    X64Reg reg = REG_RAX;
    while (!(free & (1 << reg))) reg++;
    
    emit->regs.gpr_free &= ~(1 << reg);
    emit->regs.gpr_map[reg] = (uint8_t)(ir_ref & 0xFF);
    emit->regs.ir_reg[ir_ref - IRREF_BIAS] = reg;
    
    return reg;
}

X64XmmReg ljit_emit_alloc_xmm(Emitter *emit, IRRef ir_ref) {
    uint32_t free = emit->regs.xmm_free;
    if (free == 0) {
        return XMM_NONE;
    }
    
    X64XmmReg reg = XMM0;
    while (!(free & (1 << reg))) reg++;
    
    emit->regs.xmm_free &= ~(1 << reg);
    emit->regs.xmm_map[reg] = (uint8_t)(ir_ref & 0xFF);
    
    return reg;
}

void ljit_emit_free_reg(Emitter *emit, IRRef ir_ref) {
    uint32_t idx = ir_ref - IRREF_BIAS;
    if (idx >= IR_MAX_SIZE) return;
    
    uint16_t reg = emit->regs.ir_reg[idx];
    if (reg < REG_COUNT) {
        emit->regs.gpr_free |= (1 << reg);
        emit->regs.gpr_map[reg] = 0xFF;
    }
}

void ljit_emit_spill(Emitter *emit, X64Reg reg) {
    /* 溢出到栈 */
    int32_t offset = emit->spill_offset;
    emit_mov_mr(emit, REG_RSP, offset, reg);
    emit->spill_offset += 8;
}

void ljit_emit_reload(Emitter *emit, X64Reg reg, IRRef ir_ref) {
    (void)ir_ref;
    /* 从栈恢复 */
    int32_t offset = emit->spill_offset - 8;
    emit_mov_rm(emit, reg, REG_RSP, offset);
}

/* ========================================================================
 * 序言和尾声
 * ======================================================================== */

void ljit_emit_prologue(Emitter *emit) {
    /* push rbp */
    emit_push(emit, REG_RBP);
    /* mov rbp, rsp */
    emit_mov_rr(emit, REG_RBP, REG_RSP);
    /* sub rsp, frame_size */
    if (emit->frame_size > 0) {
        emit_sub_ri(emit, REG_RSP, (int32_t)emit->frame_size);
    }
    
    /* 保存被调用者保存寄存器 */
    uint32_t saved = emit->regs.gpr_saved & CALLEE_SAVED;
    for (X64Reg r = REG_RAX; r < REG_COUNT; r++) {
        if (saved & (1 << r)) {
            emit_push(emit, r);
        }
    }
}

void ljit_emit_epilogue(Emitter *emit) {
    /* 恢复被调用者保存寄存器 */
    uint32_t saved = emit->regs.gpr_saved & CALLEE_SAVED;
    for (int r = REG_R15; r >= REG_RAX; r--) {
        if (saved & (1 << r)) {
            emit_pop(emit, (X64Reg)r);
        }
    }
    
    /* leave */
    emit_mov_rr(emit, REG_RSP, REG_RBP);
    emit_pop(emit, REG_RBP);
    /* ret */
    emit_ret(emit);
}

/* ========================================================================
 * 标签和补丁
 * ======================================================================== */

uint32_t ljit_emit_label(Emitter *emit) {
    return emit->code.label_count++;
}

void ljit_emit_bind_label(Emitter *emit, uint32_t label) {
    if (label < emit->code.label_count) {
        emit->code.labels[label] = (uint32_t)(emit->code.cur - emit->code.code);
    }
}

void ljit_emit_apply_patches(Emitter *emit) {
    for (uint32_t i = 0; i < emit->code.patch_count; i++) {
        uint32_t code_off = emit->code.patches[i].code_offset;
        uint32_t label = emit->code.patches[i].label_idx;
        int32_t target = (int32_t)emit->code.labels[label];
        int32_t offset = target - (int32_t)(code_off + 4);
        
        /* 写入偏移 */
        uint8_t *p = emit->code.code + code_off;
        p[0] = offset & 0xFF;
        p[1] = (offset >> 8) & 0xFF;
        p[2] = (offset >> 16) & 0xFF;
        p[3] = (offset >> 24) & 0xFF;
    }
}

/* ========================================================================
 * 代码生成主入口
 * ======================================================================== */

JitError ljit_emit_trace(Emitter *emit, Trace *trace) {
    emit->frame_size = 64;  /* 默认栈帧 */
    
    ljit_emit_prologue(emit);
    
    /* 遍历IR生成代码 */
    for (uint32_t i = 0; i < emit->builder->ir_cur; i++) {
        JitError err = ljit_emit_ir(emit, IRREF_BIAS + i);
        if (err != JIT_OK) return err;
    }
    
    ljit_emit_epilogue(emit);
    ljit_emit_apply_patches(emit);
    
    /* 更新trace信息 */
    trace->mcode = emit->code.code;
    trace->mcode_size = (size_t)(emit->code.cur - emit->code.code);
    
    return JIT_OK;
}

JitError ljit_emit_ir(Emitter *emit, IRRef ref) {
    IRIns *ir = &emit->builder->ir[ref - IRREF_BIAS];
    
    switch (ir->op) {
        case IR_NOP:
            break;
            
        case IR_ADD_INT: {
            X64Reg dst = ljit_emit_alloc_gpr(emit, ref);
            /* 简化：假设op1已在寄存器中 */
            emit_add_rr(emit, dst, REG_RAX);
            break;
        }
        
        case IR_SUB_INT: {
            X64Reg dst = ljit_emit_alloc_gpr(emit, ref);
            emit_sub_rr(emit, dst, REG_RAX);
            break;
        }
        
        case IR_RET:
            ljit_emit_epilogue(emit);
            break;
            
        default:
            /* NYI */
            break;
    }
    
    return JIT_OK;
}

/* ========================================================================
 * 侧边出口
 * ======================================================================== */

void ljit_emit_exit_stub(Emitter *emit, uint32_t exit_idx, uint32_t snap_idx) {
    (void)snap_idx;
    
    /* 保存当前位置 */
    if (!emit->exit_stubs) {
        emit->exit_stubs = (uint32_t *)malloc(sizeof(uint32_t) * 64);
    }
    emit->exit_stubs[exit_idx] = (uint32_t)(emit->code.cur - emit->code.code);
    
    /* 生成出口代码 - 跳转到解释器 */
    emit_mov_ri(emit, REG_RAX, (int64_t)exit_idx);
    /* 这里应该跳转到去优化处理函数 */
    emit_ret(emit);
}

void ljit_emit_exit_jump(Emitter *emit, uint32_t exit_idx) {
    /* 跳转到出口存根 */
    int32_t offset = (int32_t)emit->exit_stubs[exit_idx] - 
                     (int32_t)(emit->code.cur - emit->code.code) - 5;
    emit_jmp_rel32(emit, offset);
}

/* ========================================================================
 * 调试
 * ======================================================================== */

void ljit_emit_disasm(Emitter *emit) {
    printf("=== Generated Code (%zu bytes) ===\n",
           (size_t)(emit->code.cur - emit->code.code));
    
    uint8_t *p = emit->code.code;
    while (p < emit->code.cur) {
        printf("%04zx: ", p - emit->code.code);
        for (int i = 0; i < 8 && p + i < emit->code.cur; i++) {
            printf("%02X ", p[i]);
        }
        printf("\n");
        p += 8;
    }
}
