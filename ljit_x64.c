#define ljit_x64_c
#define LUA_CORE

#include "lprefix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "ljit_arch.h"
#include "ldo.h"
#include "ljit.h"
#include "lopcodes.h"

#if defined(__x86_64__) && defined(__linux__)

#include <sys/mman.h>
#include <unistd.h>

struct JitState {
  unsigned char *code;
  size_t size;
  size_t capacity;
};

static void emit_byte(JitState *J, unsigned char b) {
  if (J->size < J->capacity) {
    J->code[J->size++] = b;
  }
}

static void emit_u64(JitState *J, unsigned long long u) {
  for (int i = 0; i < 8; i++) {
    emit_byte(J, (u >> (i * 8)) & 0xFF);
  }
}

static void *alloc_exec_mem(size_t size) {
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) return NULL;
  return ptr;
}

JitState *jit_new_state(void) {
  JitState *J = (JitState *)malloc(sizeof(JitState));
  if (J) {
    J->code = NULL;
    J->size = 0;
    J->capacity = 0;
  }
  return J;
}

void jit_free_state(JitState *J) {
  if (J) free(J);
}

int jit_begin(JitState *J, size_t initial_size) {
  unsigned char *mem = (unsigned char *)alloc_exec_mem(initial_size);
  if (!mem) return 0;
  J->code = mem;
  J->size = 0;
  J->capacity = initial_size;
  return 1;
}

void jit_end(JitState *J, Proto *p) {
  // If we wanted to shrink the buffer, we could mremap here, but for now we keep it simple.
  p->jit_code = J->code;
  p->jit_size = J->size;
}

void jit_emit_prologue(JitState *J) {
  /*
  ** x86-64 calling convention (System V AMD64 ABI):
  ** Args: RDI, RSI, RDX, RCX, R8, R9
  ** Return: RAX
  ** Callee-saved: RBX, RBP, R12-R15
  */
  emit_byte(J, 0x55);             // push rbp
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xE5); // mov rbp, rsp

  // Save L (RDI) and ci (RSI) to callee-saved registers
  emit_byte(J, 0x53);             // push rbx
  emit_byte(J, 0x41); emit_byte(J, 0x54); // push r12

  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xFB); // mov rbx, rdi
  emit_byte(J, 0x49); emit_byte(J, 0x89); emit_byte(J, 0xF4); // mov r12, rsi
}

void jit_emit_epilogue(JitState *J) {
  emit_byte(J, 0x41); emit_byte(J, 0x5C); // pop r12
  emit_byte(J, 0x5B); // pop rbx
  emit_byte(J, 0x5D); // pop rbp
  emit_byte(J, 0xC3);             // ret
}

void jit_emit_op_return0(JitState *J) {
  // Prep stack
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xDF); // mov rdi, rbx (L)
  emit_byte(J, 0x4C); emit_byte(J, 0x89); emit_byte(J, 0xE6); // mov rsi, r12 (ci)
  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)&luaJ_prep_return0);
  emit_byte(J, 0xFF); emit_byte(J, 0xD0); // call rax

  // Call luaD_poscall(L, ci, 0)
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xDF); // mov rdi, rbx (L)
  emit_byte(J, 0x4C); emit_byte(J, 0x89); emit_byte(J, 0xE6); // mov rsi, r12 (ci)
  emit_byte(J, 0x48); emit_byte(J, 0x31); emit_byte(J, 0xD2); // xor rdx, rdx (0)

  // Load address of luaD_poscall to RAX
  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)&luaD_poscall);

  // Call rax
  emit_byte(J, 0xFF); emit_byte(J, 0xD0); // call rax

  // Set return value to 1 (success)
  emit_byte(J, 0xB8);
  emit_byte(J, 0x01); emit_byte(J, 0x00); emit_byte(J, 0x00); emit_byte(J, 0x00); // mov eax, 1

  // Epilogue sequence inline because we return
  jit_emit_epilogue(J);
}

void jit_emit_op_return1(JitState *J, int ra) {
  // Prep stack: luaJ_prep_return1(L, ci, ra)
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xDF); // mov rdi, rbx (L)
  emit_byte(J, 0x4C); emit_byte(J, 0x89); emit_byte(J, 0xE6); // mov rsi, r12 (ci)
  // mov rdx, ra
  emit_byte(J, 0x48); emit_byte(J, 0xC7); emit_byte(J, 0xC2); // mov rdx, imm32
  emit_byte(J, ra & 0xFF); emit_byte(J, (ra >> 8) & 0xFF); emit_byte(J, 0x00); emit_byte(J, 0x00);

  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)&luaJ_prep_return1);
  emit_byte(J, 0xFF); emit_byte(J, 0xD0); // call rax

  // Call luaD_poscall(L, ci, 1)
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xDF); // mov rdi, rbx (L)
  emit_byte(J, 0x4C); emit_byte(J, 0x89); emit_byte(J, 0xE6); // mov rsi, r12 (ci)
  // mov rdx, 1
  emit_byte(J, 0x48); emit_byte(J, 0xC7); emit_byte(J, 0xC2); // mov rdx, imm32
  emit_byte(J, 0x01); emit_byte(J, 0x00); emit_byte(J, 0x00); emit_byte(J, 0x00);

  // Load address of luaD_poscall to RAX
  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)&luaD_poscall);

  // Call rax
  emit_byte(J, 0xFF); emit_byte(J, 0xD0); // call rax

  // Set return value to 1 (success)
  emit_byte(J, 0xB8);
  emit_byte(J, 0x01); emit_byte(J, 0x00); emit_byte(J, 0x00); emit_byte(J, 0x00); // mov eax, 1

  jit_emit_epilogue(J);
}

static void emit_arith_common(JitState *J, int ra, int rb, int rc, const Instruction *next_pc, int op) {
  // Update savedpc: mov [r12 + 32], next_pc
  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)next_pc);

  emit_byte(J, 0x49); emit_byte(J, 0x89); emit_byte(J, 0x44); emit_byte(J, 0x24); emit_byte(J, 0x20); // mov [r12+32], rax (32 = 0x20)

  // Call luaO_arith(L, op, &rb, &rc, &ra)
  // RDI = L (rbx)
  emit_byte(J, 0x48); emit_byte(J, 0x89); emit_byte(J, 0xDF); // mov rdi, rbx

  // RSI = op
  emit_byte(J, 0x48); emit_byte(J, 0xC7); emit_byte(J, 0xC6); // mov rsi, imm32
  emit_byte(J, op & 0xFF); emit_byte(J, (op >> 8) & 0xFF); emit_byte(J, 0x00); emit_byte(J, 0x00);

  // Calculate base = [r12] + 16. ci->func.p is at offset 0 of ci (r12).
  // We need &R[x] = base + x*16.

  // RDX = &R[rb]
  // mov rdx, [r12]
  emit_byte(J, 0x49); emit_byte(J, 0x8B); emit_byte(J, 0x14); emit_byte(J, 0x24); // mov rdx, [r12]
  // add rdx, 16 + rb*16
  long long offset_rb = 16 + rb * 16;
  emit_byte(J, 0x48); emit_byte(J, 0x81); emit_byte(J, 0xC2); // add rdx, imm32
  emit_byte(J, offset_rb & 0xFF); emit_byte(J, (offset_rb >> 8) & 0xFF); emit_byte(J, (offset_rb >> 16) & 0xFF); emit_byte(J, (offset_rb >> 24) & 0xFF);

  // RCX = &R[rc]
  // mov rcx, [r12]
  emit_byte(J, 0x49); emit_byte(J, 0x8B); emit_byte(J, 0x0C); emit_byte(J, 0x24); // mov rcx, [r12]
  // add rcx, 16 + rc*16
  long long offset_rc = 16 + rc * 16;
  emit_byte(J, 0x48); emit_byte(J, 0x81); emit_byte(J, 0xC1); // add rcx, imm32
  emit_byte(J, offset_rc & 0xFF); emit_byte(J, (offset_rc >> 8) & 0xFF); emit_byte(J, (offset_rc >> 16) & 0xFF); emit_byte(J, (offset_rc >> 24) & 0xFF);

  // R8 = &R[ra]
  // mov r8, [r12]
  emit_byte(J, 0x4D); emit_byte(J, 0x8B); emit_byte(J, 0x04); emit_byte(J, 0x24); // mov r8, [r12]
  // add r8, 16 + ra*16
  long long offset_ra = 16 + ra * 16;
  emit_byte(J, 0x49); emit_byte(J, 0x81); emit_byte(J, 0xC0); // add r8, imm32
  emit_byte(J, offset_ra & 0xFF); emit_byte(J, (offset_ra >> 8) & 0xFF); emit_byte(J, (offset_ra >> 16) & 0xFF); emit_byte(J, (offset_ra >> 24) & 0xFF);

  // Call luaO_arith
  emit_byte(J, 0x48); emit_byte(J, 0xB8); // mov rax, imm64
  emit_u64(J, (unsigned long long)(uintptr_t)&luaO_arith);
  emit_byte(J, 0xFF); emit_byte(J, 0xD0); // call rax
}

void jit_emit_op_add(JitState *J, int ra, int rb, int rc, const Instruction *next_pc) {
  emit_arith_common(J, ra, rb, rc, next_pc, LUA_OPADD);
}

void jit_emit_op_sub(JitState *J, int ra, int rb, int rc, const Instruction *next_pc) {
  emit_arith_common(J, ra, rb, rc, next_pc, LUA_OPSUB);
}

void jit_free_code(Proto *p) {
  if (p->jit_code) {
    munmap(p->jit_code, JIT_BUFFER_SIZE);
    p->jit_code = NULL;
    p->jit_size = 0;
  }
}

#else

// Stubs for non-x86_64 or non-Linux
struct JitState {};

JitState *jit_new_state(void) { return NULL; }
void jit_free_state(JitState *J) { (void)J; }
int jit_begin(JitState *J, size_t initial_size) { (void)J; (void)initial_size; return 0; }
void jit_end(JitState *J, Proto *p) { (void)J; (void)p; }
void jit_emit_prologue(JitState *J) { (void)J; }
void jit_emit_epilogue(JitState *J) { (void)J; }
void jit_emit_op_return0(JitState *J) { (void)J; }
void jit_emit_op_return1(JitState *J, int ra) { (void)J; (void)ra; }
void jit_emit_op_add(JitState *J, int ra, int rb, int rc, const Instruction *next_pc) { (void)J; (void)ra; (void)rb; (void)rc; (void)next_pc; }
void jit_emit_op_sub(JitState *J, int ra, int rb, int rc, const Instruction *next_pc) { (void)J; (void)ra; (void)rb; (void)rc; (void)next_pc; }
void jit_free_code(Proto *p) { (void)p; }

#endif
