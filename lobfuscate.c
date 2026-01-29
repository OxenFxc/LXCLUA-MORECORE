/*
** $Id: lobfuscate.c $
** Control Flow Flattening Obfuscation for Lua bytecode
** DifierLine
*/

#define lobfuscate_c
#define LUA_CORE

#include "lprefix.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#include "lua.h"
#include "lobfuscate.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lobject.h"

/* 全局日志文件指针 - 由 luaO_flatten 设置 */
static FILE *g_cff_log_file = NULL;

/*
** 写入CFF调试日志
** @param fmt 格式字符串
** @param ... 可变参数
*/
static void CFF_LOG(const char *fmt, ...) {
  if (g_cff_log_file == NULL) return;
  
  va_list args;
  va_start(args, fmt);
  fprintf(g_cff_log_file, "[CFF] ");
  vfprintf(g_cff_log_file, fmt, args);
  fprintf(g_cff_log_file, "\n");
  fflush(g_cff_log_file);  /* 立即刷新确保日志写入 */
  va_end(args);
}

/* 获取操作码名称（用于调试输出） */
static const char* getOpName(OpCode op) {
  static const char* names[] = {
    "MOVE", "LOADI", "LOADF", "LOADK", "LOADKX", "LOADFALSE", "LFALSESKIP",
    "LOADTRUE", "LOADNIL", "GETUPVAL", "SETUPVAL", "GETTABUP", "GETTABLE",
    "GETI", "GETFIELD", "SETTABUP", "SETTABLE", "SETI", "SETFIELD", "NEWTABLE",
    "SELF", "ADDI", "ADDK", "SUBK", "MULK", "MODK", "POWK", "DIVK", "IDIVK",
    "BANDK", "BORK", "BXORK", "SHLI", "SHRI", "ADD", "SUB", "MUL", "MOD",
    "POW", "DIV", "IDIV", "BAND", "BOR", "BXOR", "SHL", "SHR", "SPACESHIP",
    "MMBIN", "MMBINI", "MMBINK", "UNM", "BNOT", "NOT", "LEN", "CONCAT",
    "CLOSE", "TBC", "JMP", "EQ", "LT", "LE", "EQK", "EQI", "LTI", "LEI",
    "GTI", "GEI", "TEST", "TESTSET", "CALL", "TAILCALL", "RETURN", "RETURN0",
    "RETURN1", "FORLOOP", "FORPREP", "TFORPREP", "TFORCALL", "TFORLOOP",
    "SETLIST", "CLOSURE", "VARARG", "GETVARG", "ERRNNIL", "VARARGPREP",
    "IS", "TESTNIL", "NEWCLASS", "INHERIT", "GETSUPER", "SETMETHOD",
    "SETSTATIC", "NEWOBJ", "GETPROP", "SETPROP", "INSTANCEOF", "IMPLEMENT",
    "SETIFACEFLAG", "ADDMETHOD", "SLICE", "NOP", "EXTRAARG"
  };
  if (op >= 0 && op < (int)(sizeof(names)/sizeof(names[0]))) {
    return names[op];
  }
  return "UNKNOWN";
}


/*
** =======================================================
** 内部常量定义
** =======================================================
*/

#define INITIAL_BLOCK_CAPACITY  16      /* 基本块数组初始容量 */
#define INITIAL_CODE_CAPACITY   64      /* 代码数组初始容量 */
#define CFF_MAGIC               0x43464600  /* "CFF\0" 元数据魔数 */
#define CFF_VERSION             1       /* 元数据版本号 */


/*
** =======================================================
** 辅助宏定义
** =======================================================
*/

/* 线性同余随机数生成器参数 */
#define LCG_A       1664525
#define LCG_C       1013904223

/* 生成下一个随机数 */
#define NEXT_RAND(seed) ((seed) = (LCG_A * (seed) + LCG_C))


/*
** =======================================================
** 内部辅助函数
** =======================================================
*/


/*
** 检查指令是否为基本块终结指令
** @param op 操作码
** @return 是终结指令返回1，否则返回0
**
** 终结指令包括：
** - 无条件跳转(JMP)
** - 条件跳转(EQ, LT, LE, TEST等)
** - 返回指令(RETURN, RETURN0, RETURN1, TAILCALL)
** - 循环指令(FORLOOP, FORPREP, TFORLOOP等)
*/
int luaO_isBlockTerminator (OpCode op) {
  switch (op) {
    case OP_JMP:
    case OP_EQ:
    case OP_LT:
    case OP_LE:
    case OP_EQK:
    case OP_EQI:
    case OP_LTI:
    case OP_LEI:
    case OP_GTI:
    case OP_GEI:
    case OP_TEST:
    case OP_TESTSET:
    case OP_TESTNIL:
    case OP_RETURN:
    case OP_RETURN0:
    case OP_RETURN1:
    case OP_TAILCALL:
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORPREP:
    case OP_TFORLOOP:
    case OP_TFORCALL:
      return 1;
    default:
      return 0;
  }
}


/*
** 检查指令是否为跳转指令
** @param op 操作码
** @return 是跳转指令返回1，否则返回0
*/
int luaO_isJumpInstruction (OpCode op) {
  switch (op) {
    case OP_JMP:
    case OP_FORLOOP:
    case OP_FORPREP:
    case OP_TFORPREP:
    case OP_TFORLOOP:
      return 1;
    default:
      return 0;
  }
}


/*
** 检查指令是否为条件测试指令（后面跟着跳转）
** @param op 操作码
** @return 是条件测试指令返回1，否则返回0
*/
static int isConditionalTest (OpCode op) {
  switch (op) {
    case OP_EQ:
    case OP_LT:
    case OP_LE:
    case OP_EQK:
    case OP_EQI:
    case OP_LTI:
    case OP_LEI:
    case OP_GTI:
    case OP_GEI:
    case OP_TEST:
    case OP_TESTSET:
    case OP_TESTNIL:
      return 1;
    default:
      return 0;
  }
}


/*
** 检查指令是否为返回指令
** @param op 操作码
** @return 是返回指令返回1，否则返回0
*/
static int isReturnInstruction (OpCode op) {
  switch (op) {
    case OP_RETURN:
    case OP_RETURN0:
    case OP_RETURN1:
    case OP_TAILCALL:
      return 1;
    default:
      return 0;
  }
}


/*
** 获取跳转指令的目标PC
** @param inst 指令
** @param pc 当前PC
** @return 跳转目标PC，如果不是跳转指令返回-1
*/
int luaO_getJumpTarget (Instruction inst, int pc) {
  OpCode op = GET_OPCODE(inst);
  switch (op) {
    case OP_JMP:
      return pc + 1 + GETARG_sJ(inst);
    case OP_FORLOOP:
    case OP_TFORLOOP:
      return pc + 1 - GETARG_Bx(inst);
    case OP_FORPREP:
    case OP_TFORPREP:
      return pc + 1 + GETARG_Bx(inst);
    default:
      return -1;
  }
}


/*
** 初始化扁平化上下文
** @param L Lua状态
** @param f 函数原型
** @param flags 混淆标志
** @param seed 随机种子
** @return 初始化的上下文，失败返回NULL
*/
static CFFContext *initContext (lua_State *L, Proto *f, int flags, unsigned int seed) {
  CFFContext *ctx = (CFFContext *)luaM_malloc_(L, sizeof(CFFContext), 0);
  if (ctx == NULL) return NULL;
  
  ctx->L = L;
  ctx->f = f;
  ctx->num_blocks = 0;
  ctx->block_capacity = INITIAL_BLOCK_CAPACITY;
  ctx->new_code = NULL;
  ctx->new_code_size = 0;
  ctx->new_code_capacity = 0;
  ctx->state_reg = f->maxstacksize;  /* 使用一个新的寄存器作为状态变量 */
  ctx->dispatcher_pc = 0;
  ctx->seed = seed;
  ctx->obfuscate_flags = flags;
  
  /* 分配基本块数组 */
  ctx->blocks = (BasicBlock *)luaM_malloc_(L, sizeof(BasicBlock) * ctx->block_capacity, 0);
  if (ctx->blocks == NULL) {
    luaM_free_(L, ctx, sizeof(CFFContext));
    return NULL;
  }
  
  return ctx;
}


/*
** 释放扁平化上下文
** @param ctx 要释放的上下文
*/
static void freeContext (CFFContext *ctx) {
  if (ctx == NULL) return;
  
  lua_State *L = ctx->L;
  
  if (ctx->blocks != NULL) {
    luaM_free_(L, ctx->blocks, sizeof(BasicBlock) * ctx->block_capacity);
  }
  
  if (ctx->new_code != NULL) {
    luaM_free_(L, ctx->new_code, sizeof(Instruction) * ctx->new_code_capacity);
  }
  
  luaM_free_(L, ctx, sizeof(CFFContext));
}


/*
** 添加基本块到上下文
** @param ctx 上下文
** @param start_pc 起始PC
** @param end_pc 结束PC
** @return 新块的索引，失败返回-1
*/
static int addBlock (CFFContext *ctx, int start_pc, int end_pc) {
  /* 检查是否需要扩展数组 */
  if (ctx->num_blocks >= ctx->block_capacity) {
    int new_capacity = ctx->block_capacity * 2;
    BasicBlock *new_blocks = (BasicBlock *)luaM_realloc_(
      ctx->L, ctx->blocks, 
      sizeof(BasicBlock) * ctx->block_capacity,
      sizeof(BasicBlock) * new_capacity
    );
    if (new_blocks == NULL) return -1;
    ctx->blocks = new_blocks;
    ctx->block_capacity = new_capacity;
  }
  
  int idx = ctx->num_blocks++;
  BasicBlock *block = &ctx->blocks[idx];
  block->start_pc = start_pc;
  block->end_pc = end_pc;
  block->state_id = idx;  /* 初始状态ID等于块索引 */
  block->original_target = -1;
  block->fall_through = -1;
  block->cond_target = -1;
  block->is_entry = (start_pc == 0);
  block->is_exit = 0;
  
  return idx;
}


/*
** 查找包含指定PC的基本块
** @param ctx 上下文
** @param pc 目标PC
** @return 块索引，未找到返回-1
*/
static int findBlockByPC (CFFContext *ctx, int pc) {
  for (int i = 0; i < ctx->num_blocks; i++) {
    if (pc >= ctx->blocks[i].start_pc && pc < ctx->blocks[i].end_pc) {
      return i;
    }
  }
  return -1;
}


/*
** 查找起始于指定PC的基本块
** @param ctx 上下文
** @param pc 目标PC
** @return 块索引，未找到返回-1
*/
static int findBlockStartingAt (CFFContext *ctx, int pc) {
  for (int i = 0; i < ctx->num_blocks; i++) {
    if (ctx->blocks[i].start_pc == pc) {
      return i;
    }
  }
  return -1;
}


/*
** =======================================================
** 基本块识别
** =======================================================
*/


/*
** 识别并构建基本块
** @param ctx 扁平化上下文
** @return 成功返回0，失败返回错误码
**
** 算法：
** 1. 第一遍扫描：识别所有基本块的起始点
**    - 函数入口(PC=0)是起始点
**    - 跳转目标是起始点
**    - 跳转指令的下一条是起始点
** 2. 第二遍扫描：根据起始点划分基本块
** 3. 分析每个基本块的出口（跳转目标、顺序执行目标）
*/
int luaO_identifyBlocks (CFFContext *ctx) {
  Proto *f = ctx->f;
  int code_size = f->sizecode;
  
  CFF_LOG("========== 开始识别基本块 ==========");
  CFF_LOG("函数代码大小: %d 条指令", code_size);
  
  /* 打印原始指令 */
  CFF_LOG("--- 原始指令序列 ---");
  for (int pc = 0; pc < code_size; pc++) {
    Instruction inst = f->code[pc];
    OpCode op = GET_OPCODE(inst);
    int a = GETARG_A(inst);
    CFF_LOG("  [%03d] %s (A=%d, raw=0x%016llx)", pc, getOpName(op), a, (unsigned long long)inst);
  }
  
  /* 创建标记数组，标记哪些PC是基本块起始点 */
  if (code_size <= 0) return -1;
  lu_byte *is_leader = (lu_byte *)luaM_malloc_(ctx->L, (size_t)code_size, 0);
  if (is_leader == NULL) return -1;
  memset(is_leader, 0, (size_t)code_size);
  
  /* 第一条指令是入口点 */
  is_leader[0] = 1;
  
  /* 第一遍扫描：识别基本块起始点 */
  for (int pc = 0; pc < code_size; pc++) {
    Instruction inst = f->code[pc];
    OpCode op = GET_OPCODE(inst);
    
    /* 跳转指令的目标是起始点 */
    if (luaO_isJumpInstruction(op)) {
      int target = luaO_getJumpTarget(inst, pc);
      if (target >= 0 && target < code_size) {
        is_leader[target] = 1;
      }
      /* 跳转指令的下一条也是起始点（除非是无条件跳转或函数结束） */
      if (pc + 1 < code_size && op != OP_JMP) {
        is_leader[pc + 1] = 1;
      }
    }
    
    /* 条件测试指令后面跟着跳转，跳转后的下一条是起始点 */
    if (isConditionalTest(op)) {
      /* 条件测试指令会跳过下一条指令（通常是JMP） */
      if (pc + 2 < code_size) {
        is_leader[pc + 2] = 1;
      }
    }
    
    /* 返回指令的下一条是起始点（如果存在） */
    if (isReturnInstruction(op)) {
      if (pc + 1 < code_size) {
        is_leader[pc + 1] = 1;
      }
    }
  }
  
  /* 第二遍扫描：根据起始点划分基本块 */
  CFF_LOG("--- 划分基本块 ---");
  int block_start = 0;
  for (int pc = 1; pc <= code_size; pc++) {
    /* 当遇到新的起始点或代码结束时，创建一个基本块 */
    if (pc == code_size || is_leader[pc]) {
      int idx = addBlock(ctx, block_start, pc);
      if (idx < 0) {
        luaM_free_(ctx->L, is_leader, code_size);
        return -1;
      }
      CFF_LOG("  块 %d: PC [%d, %d) (state_id=%d)", idx, block_start, pc, ctx->blocks[idx].state_id);
      block_start = pc;
    }
  }
  
  /* 第三遍：分析每个基本块的出口 */
  CFF_LOG("--- 分析基本块出口 ---");
  for (int i = 0; i < ctx->num_blocks; i++) {
    BasicBlock *block = &ctx->blocks[i];
    int last_pc = block->end_pc - 1;
    
    if (last_pc < 0 || last_pc >= code_size) continue;
    
    Instruction inst = f->code[last_pc];
    OpCode op = GET_OPCODE(inst);
    
    CFF_LOG("  块 %d 的最后指令 [%d]: %s", i, last_pc, getOpName(op));
    
    /* 检查是否为出口块 */
    if (isReturnInstruction(op)) {
      block->is_exit = 1;
      CFF_LOG("    -> 标记为出口块 (返回指令)");
    }
    
    /* 分析跳转目标 */
    if (luaO_isJumpInstruction(op)) {
      int target = luaO_getJumpTarget(inst, last_pc);
      if (target >= 0) {
        int target_block = findBlockStartingAt(ctx, target);
        block->original_target = target_block;
        CFF_LOG("    -> 跳转目标 PC=%d, 对应块 %d", target, target_block);
        
        /* 对于非无条件跳转，设置顺序执行目标 */
        if (op != OP_JMP) {
          int next_block = findBlockStartingAt(ctx, block->end_pc);
          block->fall_through = next_block;
          CFF_LOG("    -> 顺序执行目标块 %d", next_block);
        }
      }
    }
    
    /* 条件测试指令 */
    if (isConditionalTest(op)) {
      /* 条件为真时跳过下一条指令 */
      int skip_target = findBlockStartingAt(ctx, last_pc + 2);
      block->cond_target = skip_target;
      /* 条件为假时执行下一条（通常是JMP） */
      block->fall_through = findBlockStartingAt(ctx, block->end_pc);
      CFF_LOG("    -> 条件测试: 真->块%d (跳过JMP), 假->块%d (执行JMP)", 
              skip_target, block->fall_through);
    }
    
    /* 普通顺序执行 */
    if (!luaO_isBlockTerminator(op) && block->end_pc < code_size) {
      block->fall_through = findBlockStartingAt(ctx, block->end_pc);
      CFF_LOG("    -> 顺序执行到块 %d", block->fall_through);
    }
  }
  
  CFF_LOG("========== 基本块识别完成，共 %d 个块 ==========", ctx->num_blocks);
  
  luaM_free_(ctx->L, is_leader, code_size);
  return 0;
}


/*
** =======================================================
** 基本块打乱
** =======================================================
*/


/*
** 随机打乱基本块顺序
** @param ctx 扁平化上下文
**
** 使用Fisher-Yates算法打乱基本块顺序，
** 但保持入口块在第一位
*/
void luaO_shuffleBlocks (CFFContext *ctx) {
  if (ctx->num_blocks <= 2) return;  /* 块太少，不需要打乱 */
  
  unsigned int seed = ctx->seed;
  
  /* 从索引1开始打乱（保持入口块位置） */
  for (int i = ctx->num_blocks - 1; i > 1; i--) {
    NEXT_RAND(seed);
    int j = 1 + (seed % i);  /* j in [1, i) */
    
    /* 交换块i和块j的状态ID（不交换块本身，只交换执行顺序） */
    int temp = ctx->blocks[i].state_id;
    ctx->blocks[i].state_id = ctx->blocks[j].state_id;
    ctx->blocks[j].state_id = temp;
  }
  
  ctx->seed = seed;
}


/*
** =======================================================
** 状态编码
** =======================================================
*/


/*
** 编码状态值（增加混淆程度）
** @param state 原始状态值
** @param seed 随机种子
** @return 编码后的状态值
**
** 使用简单的线性变换进行编码，确保结果在 sC 参数范围内
** EQI 指令的 sC 参数是 16 位有符号数，范围 [-32767, 32768]
** 
** 为确保不同状态映射到不同编码值，使用基于种子的偏移和乘法：
** encoded = (state * prime + offset) mod range
** 其中 prime 与 range 互质，确保映射是一一对应的
*/
int luaO_encodeState (int state, unsigned int seed) {
  /* 使用固定范围和与之互质的乘数 */
  const int range = 30000;  /* 安全范围 */
  const int prime = 7919;   /* 质数，与 range 互质 */
  
  /* 使用种子生成偏移量 */
  int offset = (int)(seed % range);
  
  /* 线性变换：(state * prime + offset) mod range */
  /* 由于 prime 与 range 互质，这是一个置换（双射） */
  int encoded = ((state * prime) % range + offset) % range;
  if (encoded < 0) encoded += range;
  
  return encoded;
}


/*
** 解码状态值
** @param encoded_state 编码后的状态值
** @param seed 随机种子
** @return 原始状态值
**
** 这需要模逆运算，为简化实现，我们存储映射表
*/
int luaO_decodeState (int encoded_state, unsigned int seed) {
  /* 这个函数需要在元数据中存储映射表来实现 */
  /* 暂时返回原值，实际实现在反扁平化时使用映射表 */
  (void)seed;
  return encoded_state;
}


/*
** =======================================================
** 代码生成
** =======================================================
*/


/*
** 确保新代码数组有足够的空间
** @param ctx 上下文
** @param needed 需要的额外空间
** @return 成功返回0，失败返回-1
*/
static int ensureCodeCapacity (CFFContext *ctx, int needed) {
  int required = ctx->new_code_size + needed;
  
  if (required <= ctx->new_code_capacity) {
    return 0;
  }
  
  int new_capacity = ctx->new_code_capacity == 0 ? INITIAL_CODE_CAPACITY : ctx->new_code_capacity;
  while (new_capacity < required) {
    new_capacity *= 2;
  }
  
  Instruction *new_code;
  if (ctx->new_code == NULL) {
    new_code = (Instruction *)luaM_malloc_(ctx->L, sizeof(Instruction) * new_capacity, 0);
  } else {
    new_code = (Instruction *)luaM_realloc_(
      ctx->L, ctx->new_code,
      sizeof(Instruction) * ctx->new_code_capacity,
      sizeof(Instruction) * new_capacity
    );
  }
  
  if (new_code == NULL) return -1;
  
  ctx->new_code = new_code;
  ctx->new_code_capacity = new_capacity;
  return 0;
}


/*
** 添加指令到新代码
** @param ctx 上下文
** @param inst 指令
** @return 指令的PC，失败返回-1
*/
static int emitInstruction (CFFContext *ctx, Instruction inst) {
  if (ensureCodeCapacity(ctx, 1) != 0) return -1;
  
  int pc = ctx->new_code_size++;
  ctx->new_code[pc] = inst;
  return pc;
}


/*
** =======================================================
** 虚假块生成
** =======================================================
*/

/* 虚假块数量（基于真实块数量的倍数） */
#define BOGUS_BLOCK_RATIO  2  /* 每个真实块生成2个虚假块 */
#define BOGUS_BLOCK_MIN_INSTS  3  /* 虚假块最少指令数 */
#define BOGUS_BLOCK_MAX_INSTS  8  /* 虚假块最多指令数 */


/*
** 生成一条随机的虚假指令
** @param ctx 上下文
** @param seed 随机种子指针（会被更新）
** @return 生成的指令
**
** 虚假指令类型：
** - LOADI: 加载随机整数到寄存器
** - ADDI: 寄存器加立即数
** - MOVE: 寄存器间移动
** - LOADK: 加载常量（如果有常量）
*/
static Instruction generateBogusInstruction (CFFContext *ctx, unsigned int *seed) {
  int state_reg = ctx->state_reg;
  int max_reg = state_reg;  /* 使用状态寄存器之前的寄存器 */
  
  NEXT_RAND(*seed);
  int inst_type = *seed % 4;
  
  NEXT_RAND(*seed);
  int reg = (*seed % max_reg);  /* 目标寄存器 */
  if (reg < 0) reg = 0;
  
  NEXT_RAND(*seed);
  int value = (*seed % 1000) - 500;  /* -500 ~ 499 */
  
  switch (inst_type) {
    case 0:  /* LOADI reg, value */
      return CREATE_ABx(OP_LOADI, reg, value + OFFSET_sBx);
    
    case 1:  /* ADDI reg, reg, value */
      return CREATE_ABCk(OP_ADDI, reg, reg, int2sC(value % 100), 0);
    
    case 2:  /* MOVE reg, other_reg */
      {
        NEXT_RAND(*seed);
        int src_reg = (*seed % max_reg);
        if (src_reg < 0) src_reg = 0;
        return CREATE_ABCk(OP_MOVE, reg, src_reg, 0, 0);
      }
    
    default:  /* LOADI with different value */
      NEXT_RAND(*seed);
      return CREATE_ABx(OP_LOADI, reg, (*seed % 2000) + OFFSET_sBx);
  }
}


/*
** 生成一个虚假基本块的代码
** @param ctx 上下文
** @param bogus_state 虚假块的状态ID
** @param seed 随机种子指针
** @return 成功返回0，失败返回-1
**
** 虚假块结构：
** - 3~8条随机的算术/移动指令
** - 设置状态为另一个随机虚假状态或跳回分发器
** - JMP 回分发器
*/
static int emitBogusBlock (CFFContext *ctx, int bogus_state, unsigned int *seed) {
  int state_reg = ctx->state_reg;
  
  /* 决定虚假块的指令数量 */
  NEXT_RAND(*seed);
  int num_insts = BOGUS_BLOCK_MIN_INSTS + (*seed % (BOGUS_BLOCK_MAX_INSTS - BOGUS_BLOCK_MIN_INSTS + 1));
  
  CFF_LOG("  生成虚假块: state=%d, 指令数=%d", bogus_state, num_insts);
  
  /* 生成随机指令 */
  for (int i = 0; i < num_insts; i++) {
    Instruction bogus_inst = generateBogusInstruction(ctx, seed);
    if (emitInstruction(ctx, bogus_inst) < 0) return -1;
  }
  
  /* 生成下一个状态（指向另一个虚假块或回到分发器循环） */
  NEXT_RAND(*seed);
  int next_state = bogus_state + 1 + (*seed % 3);  /* 指向附近的状态 */
  
  if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
    next_state = luaO_encodeState(next_state, ctx->seed);
  }
  
  /* LOADI state_reg, next_state */
  Instruction state_inst = CREATE_ABx(OP_LOADI, state_reg, next_state + OFFSET_sBx);
  if (emitInstruction(ctx, state_inst) < 0) return -1;
  
  /* JMP back to dispatcher */
  int jmp_offset = ctx->dispatcher_pc - ctx->new_code_size - 1;
  Instruction jmp_inst = CREATE_sJ(OP_JMP, jmp_offset + OFFSET_sJ, 0);
  if (emitInstruction(ctx, jmp_inst) < 0) return -1;
  
  return 0;
}


/*
** 生成dispatcher代码
** @param ctx 扁平化上下文
** @return 成功返回0，失败返回错误码
**
** Dispatcher结构：
**   LOADI state_reg, initial_state   ; 初始化状态
** dispatcher_loop:
**   ; 对每个状态生成比较和跳转（包括真实块和虚假块）
**   EQI state_reg, state_0, k=1
**   JMP block_0
**   EQI state_reg, state_1, k=1
**   JMP block_1
**   ...
**   JMP dispatcher_loop              ; 默认跳回循环
*/
int luaO_generateDispatcher (CFFContext *ctx) {
  if (ctx->num_blocks == 0) return 0;
  
  int state_reg = ctx->state_reg;
  unsigned int bogus_seed = ctx->seed;
  
  CFF_LOG("========== 开始生成扁平化代码 ==========");
  CFF_LOG("状态寄存器: R[%d]", state_reg);
  
  /* 计算虚假块数量 */
  int num_bogus_blocks = 0;
  if (ctx->obfuscate_flags & OBFUSCATE_BOGUS_BLOCKS) {
    num_bogus_blocks = ctx->num_blocks * BOGUS_BLOCK_RATIO;
    CFF_LOG("启用虚假块: 将生成 %d 个虚假块", num_bogus_blocks);
  }
  
  int total_blocks = ctx->num_blocks + num_bogus_blocks;
  
  /* 生成初始化状态的指令 */
  /* 入口块的状态ID */
  int entry_state = 0;
  for (int i = 0; i < ctx->num_blocks; i++) {
    if (ctx->blocks[i].is_entry) {
      entry_state = ctx->blocks[i].state_id;
      CFF_LOG("入口块: 块%d, state_id=%d", i, entry_state);
      break;
    }
  }
  
  /* 如果启用了状态编码，编码初始状态 */
  if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
    entry_state = luaO_encodeState(entry_state, ctx->seed);
  }
  
  /* LOADI state_reg, entry_state */
  CFF_LOG("生成初始化指令: LOADI R[%d], %d", state_reg, entry_state);
  Instruction init_inst = CREATE_ABx(OP_LOADI, state_reg, entry_state + OFFSET_sBx);
  if (emitInstruction(ctx, init_inst) < 0) return -1;
  
  /* 记录dispatcher位置 */
  ctx->dispatcher_pc = ctx->new_code_size;
  CFF_LOG("分发器起始位置: PC=%d", ctx->dispatcher_pc);
  
  /* 为所有块（真实块+虚假块）分配跳转PC数组 */
  int *all_block_jmp_pcs = (int *)luaM_malloc_(ctx->L, sizeof(int) * total_blocks, 0);
  if (all_block_jmp_pcs == NULL) return -1;
  
  /* 为虚假块生成状态ID（从 num_blocks 开始） */
  int *bogus_states = NULL;
  if (num_bogus_blocks > 0) {
    bogus_states = (int *)luaM_malloc_(ctx->L, sizeof(int) * num_bogus_blocks, 0);
    if (bogus_states == NULL) {
      luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
      return -1;
    }
    for (int i = 0; i < num_bogus_blocks; i++) {
      bogus_states[i] = ctx->num_blocks + i;
    }
  }
  
  /* 生成状态比较代码 - 真实块 */
  CFF_LOG("--- 生成状态比较代码（真实块）---");
  for (int i = 0; i < ctx->num_blocks; i++) {
    int state = ctx->blocks[i].state_id;
    
    if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
      state = luaO_encodeState(state, ctx->seed);
    }
    
    CFF_LOG("  [PC=%d] EQI R[%d], %d, k=1 (真实块%d)", 
            ctx->new_code_size, state_reg, state, i);
    Instruction cmp_inst = CREATE_ABCk(OP_EQI, state_reg, int2sC(state), 0, 1);
    if (emitInstruction(ctx, cmp_inst) < 0) {
      luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
      if (bogus_states) luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
      return -1;
    }
    
    CFF_LOG("  [PC=%d] JMP -> 真实块%d (偏移量待定)", ctx->new_code_size, i);
    Instruction jmp_inst = CREATE_sJ(OP_JMP, 0, 0);
    int jmp_pc = emitInstruction(ctx, jmp_inst);
    if (jmp_pc < 0) {
      luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
      if (bogus_states) luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
      return -1;
    }
    all_block_jmp_pcs[i] = jmp_pc;
  }
  
  /* 生成状态比较代码 - 虚假块 */
  if (num_bogus_blocks > 0) {
    CFF_LOG("--- 生成状态比较代码（虚假块）---");
    for (int i = 0; i < num_bogus_blocks; i++) {
      int state = bogus_states[i];
      
      if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
        state = luaO_encodeState(state, ctx->seed);
      }
      
      CFF_LOG("  [PC=%d] EQI R[%d], %d, k=1 (虚假块%d)", 
              ctx->new_code_size, state_reg, state, i);
      Instruction cmp_inst = CREATE_ABCk(OP_EQI, state_reg, int2sC(state), 0, 1);
      if (emitInstruction(ctx, cmp_inst) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
        luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
        return -1;
      }
      
      CFF_LOG("  [PC=%d] JMP -> 虚假块%d (偏移量待定)", ctx->new_code_size, i);
      Instruction jmp_inst = CREATE_sJ(OP_JMP, 0, 0);
      int jmp_pc = emitInstruction(ctx, jmp_inst);
      if (jmp_pc < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
        luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
        return -1;
      }
      all_block_jmp_pcs[ctx->num_blocks + i] = jmp_pc;
    }
  }
  
  /* 生成默认跳转回dispatcher的指令 */
  int dispatcher_end = ctx->new_code_size;
  Instruction loop_jmp = CREATE_sJ(OP_JMP, ctx->dispatcher_pc - dispatcher_end - 1 + OFFSET_sJ, 0);
  if (emitInstruction(ctx, loop_jmp) < 0) {
    luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
    if (bogus_states) luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
    return -1;
  }
  
  /* 记录所有块的代码起始位置 */
  int *all_block_starts = (int *)luaM_malloc_(ctx->L, sizeof(int) * total_blocks, 0);
  if (all_block_starts == NULL) {
    luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * total_blocks);
    if (bogus_states) luaM_free_(ctx->L, bogus_states, sizeof(int) * num_bogus_blocks);
    return -1;
  }
  
  /* 复制原始基本块代码，并记录新的起始位置 */
  CFF_LOG("--- 复制基本块代码 ---");
  Proto *f = ctx->f;
  
  for (int i = 0; i < ctx->num_blocks; i++) {
    BasicBlock *block = &ctx->blocks[i];
    all_block_starts[i] = ctx->new_code_size;
    
    CFF_LOG("块 %d: 原始PC [%d, %d), 新起始PC=%d, state_id=%d", 
            i, block->start_pc, block->end_pc, all_block_starts[i], block->state_id);
    
    /* 分析基本块的最后指令 */
    int last_pc = block->end_pc - 1;
    OpCode last_op = OP_NOP;  /* 空操作码作为默认值 */
    int has_cond_test = 0;
    int cond_test_pc = -1;
    
    if (last_pc >= block->start_pc) {
      last_op = GET_OPCODE(f->code[last_pc]);
      
      /* 检查是否有条件测试+JMP模式 */
      /* 条件测试指令在倒数第二条，JMP在最后一条 */
      if (last_op == OP_JMP && last_pc > block->start_pc) {
        OpCode prev_op = GET_OPCODE(f->code[last_pc - 1]);
        if (isConditionalTest(prev_op)) {
          has_cond_test = 1;
          cond_test_pc = last_pc - 1;
          CFF_LOG("  检测到条件测试+JMP模式: %s @ PC=%d, JMP @ PC=%d", 
                  getOpName(prev_op), cond_test_pc, last_pc);
        }
      }
    }
    
    /* 复制基本块的指令 */
    int copy_end = block->end_pc;
    
    if (has_cond_test) {
      /* 条件分支块：复制到条件测试指令之前（不含） */
      copy_end = cond_test_pc;
    } else if (last_op == OP_JMP) {
      /* 无条件跳转块：不复制最后的JMP */
      copy_end = block->end_pc - 1;
    }
    /* 其他情况（包括返回指令）：复制全部 */
    
    /* 复制指令 */
    for (int pc = block->start_pc; pc < copy_end; pc++) {
      if (emitInstruction(ctx, f->code[pc]) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
    }
    
    /* 处理基本块的出口 */
    if (block->is_exit) {
      /* 出口块：复制返回指令（如果还没复制） */
      if (copy_end < block->end_pc) {
        for (int pc = copy_end; pc < block->end_pc; pc++) {
          if (emitInstruction(ctx, f->code[pc]) < 0) {
            luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
            luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
            return -1;
          }
        }
      }
    } else if (has_cond_test) {
      /* 条件分支块：需要生成两个分支的状态转换 */
      
      /* 复制条件测试指令 */
      Instruction cond_inst = f->code[cond_test_pc];
      OpCode cond_op = GET_OPCODE(cond_inst);
      int cond_k = getarg(cond_inst, POS_k, 1);
      CFF_LOG("  复制条件测试: %s (k=%d) @ 新PC=%d", getOpName(cond_op), cond_k, ctx->new_code_size);
      if (emitInstruction(ctx, cond_inst) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
      /* 
      ** Lua条件测试指令的语义：if (cond ~= k) then pc++
      ** 当k=0时：条件为真则跳过下一条指令
      ** 
      ** 原始代码结构：
      **   [条件测试]  ; 条件为真时跳过JMP
      **   JMP else    ; 条件为假时跳到else
      **   ; then分支
      **   ...
      **   ; else分支
      **   ...
      **
      ** 生成的CFF代码结构：
      **   [条件测试]  ; 条件为真时跳过下一条JMP
      **   JMP +2      ; 条件为假时跳过then分支的状态设置
      **   LOADI state_reg, then_state  ; then分支（条件为真时执行）
      **   JMP dispatcher
      **   LOADI state_reg, else_state  ; else分支（条件为假时执行）
      **   JMP dispatcher
      */
      
      /* 获取JMP指令的原始目标（else分支） */
      Instruction orig_jmp = f->code[last_pc];
      int orig_jmp_offset = GETARG_sJ(orig_jmp);
      int orig_jmp_target = luaO_getJumpTarget(orig_jmp, last_pc);
      int else_block = findBlockStartingAt(ctx, orig_jmp_target);
      
      /* then分支是JMP后面的代码块 */
      int then_block = findBlockStartingAt(ctx, last_pc + 1);
      if (then_block < 0) then_block = block->fall_through;
      
      CFF_LOG("  原始JMP: offset=%d, 目标PC=%d", orig_jmp_offset, orig_jmp_target);
      CFF_LOG("  then分支: 块%d (PC=%d后的代码)", then_block, last_pc);
      CFF_LOG("  else分支: 块%d (JMP目标)", else_block);
      
      int then_state = then_block >= 0 ? ctx->blocks[then_block].state_id : 0;
      int else_state = else_block >= 0 ? ctx->blocks[else_block].state_id : 0;
      
      CFF_LOG("  then_state=%d, else_state=%d", then_state, else_state);
      
      if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
        then_state = luaO_encodeState(then_state, ctx->seed);
        else_state = luaO_encodeState(else_state, ctx->seed);
      }
      
      /* 生成：JMP +2 (条件为假时跳过then分支的状态设置，跳到else分支) */
      /* 
      ** 代码结构：
      ** [当前PC]   JMP +2      <- 我们在这里
      ** [当前PC+1] LOADI then  <- 跳过这条
      ** [当前PC+2] JMP disp    <- 跳过这条
      ** [当前PC+3] LOADI else  <- 跳到这里 (当前PC + 1 + 2 = 当前PC + 3)
      */
      CFF_LOG("  生成: JMP +2 (跳过then状态设置) @ 新PC=%d", ctx->new_code_size);
      Instruction skip_then = CREATE_sJ(OP_JMP, 2 + OFFSET_sJ, 0);
      if (emitInstruction(ctx, skip_then) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
      /* then分支的状态设置（条件为真时执行） */
      CFF_LOG("  生成: LOADI R[%d], %d (then状态) @ 新PC=%d", state_reg, then_state, ctx->new_code_size);
      Instruction then_state_inst = CREATE_ABx(OP_LOADI, state_reg, then_state + OFFSET_sBx);
      if (emitInstruction(ctx, then_state_inst) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
      /* 跳转到dispatcher */
      int jmp_offset1 = ctx->dispatcher_pc - ctx->new_code_size - 1;
      CFF_LOG("  生成: JMP dispatcher (offset=%d) @ 新PC=%d", jmp_offset1, ctx->new_code_size);
      Instruction jmp_disp1 = CREATE_sJ(OP_JMP, jmp_offset1 + OFFSET_sJ, 0);
      if (emitInstruction(ctx, jmp_disp1) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
      /* else分支的状态设置（条件为假时执行） */
      CFF_LOG("  生成: LOADI R[%d], %d (else状态) @ 新PC=%d", state_reg, else_state, ctx->new_code_size);
      Instruction else_state_inst = CREATE_ABx(OP_LOADI, state_reg, else_state + OFFSET_sBx);
      if (emitInstruction(ctx, else_state_inst) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
      /* 跳转到dispatcher */
      int jmp_offset2 = ctx->dispatcher_pc - ctx->new_code_size - 1;
      CFF_LOG("  生成: JMP dispatcher (offset=%d) @ 新PC=%d", jmp_offset2, ctx->new_code_size);
      Instruction jmp_disp2 = CREATE_sJ(OP_JMP, jmp_offset2 + OFFSET_sJ, 0);
      if (emitInstruction(ctx, jmp_disp2) < 0) {
        luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
        luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
        return -1;
      }
      
    } else {
      /* 普通块：无条件跳转或顺序执行 */
      int next_state = -1;
      
      if (block->original_target >= 0) {
        next_state = ctx->blocks[block->original_target].state_id;
        CFF_LOG("  普通块: 跳转到块%d (state=%d)", block->original_target, next_state);
      } else if (block->fall_through >= 0) {
        next_state = ctx->blocks[block->fall_through].state_id;
        CFF_LOG("  普通块: 顺序执行到块%d (state=%d)", block->fall_through, next_state);
      }
      
      if (next_state >= 0) {
        if (ctx->obfuscate_flags & OBFUSCATE_STATE_ENCODE) {
          next_state = luaO_encodeState(next_state, ctx->seed);
        }
        
        /* LOADI state_reg, next_state */
        CFF_LOG("  生成: LOADI R[%d], %d @ 新PC=%d", state_reg, next_state, ctx->new_code_size);
        Instruction state_inst = CREATE_ABx(OP_LOADI, state_reg, next_state + OFFSET_sBx);
        if (emitInstruction(ctx, state_inst) < 0) {
          luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
          luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
          return -1;
        }
        
        /* JMP dispatcher */
        int jmp_offset = ctx->dispatcher_pc - ctx->new_code_size - 1;
        CFF_LOG("  生成: JMP dispatcher (offset=%d) @ 新PC=%d", jmp_offset, ctx->new_code_size);
        Instruction jmp_inst = CREATE_sJ(OP_JMP, jmp_offset + OFFSET_sJ, 0);
        if (emitInstruction(ctx, jmp_inst) < 0) {
          luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
          luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
          return -1;
        }
      } else {
        CFF_LOG("  普通块: 无后继块（可能是出口块）");
      }
    }
  }
  
  /* 修正dispatcher中的跳转偏移量 */
  CFF_LOG("--- 修正分发器跳转偏移 ---");
  for (int i = 0; i < ctx->num_blocks; i++) {
    int jmp_pc = all_block_jmp_pcs[i];
    int target_pc = all_block_starts[i];
    int offset = target_pc - jmp_pc - 1;
    
    CFF_LOG("  块%d: JMP@PC=%d -> 目标PC=%d, offset=%d", i, jmp_pc, target_pc, offset);
    SETARG_sJ(ctx->new_code[jmp_pc], offset);
  }
  
  CFF_LOG("========== 扁平化代码生成完成，共 %d 条指令 ==========", ctx->new_code_size);
  
  luaM_free_(ctx->L, all_block_jmp_pcs, sizeof(int) * ctx->num_blocks);
  luaM_free_(ctx->L, all_block_starts, sizeof(int) * ctx->num_blocks);
  
  return 0;
}


/*
** =======================================================
** 公共API实现
** =======================================================
*/


/*
** 对函数原型进行控制流扁平化
** @param L Lua状态
** @param f 要处理的函数原型
** @param flags 混淆标志位组合
** @param seed 随机种子
** @param log_path 调试日志输出路径（NULL表示不输出日志）
** @return 成功返回0，失败返回错误码
*/
int luaO_flatten (lua_State *L, Proto *f, int flags, unsigned int seed,
                  const char *log_path) {
  /* 调试：输出 log_path 值 */
  fprintf(stderr, "[CFF DEBUG] luaO_flatten called, log_path=%s, flags=%d\n", 
          log_path ? log_path : "(null)", flags);
  fflush(stderr);
  
  /* 设置日志文件 */
  FILE *log_file = NULL;
  if (log_path != NULL) {
    fprintf(stderr, "[CFF DEBUG] Attempting to open log file: %s\n", log_path);
    fflush(stderr);
    log_file = fopen(log_path, "w");
    if (log_file != NULL) {
      fprintf(stderr, "[CFF DEBUG] Log file opened successfully\n");
      fflush(stderr);
      g_cff_log_file = log_file;
      CFF_LOG("======================================");
      CFF_LOG("CFF 控制流扁平化调试日志");
      CFF_LOG("======================================");
    } else {
      fprintf(stderr, "[CFF DEBUG] Failed to open log file!\n");
      fflush(stderr);
    }
  }
  
  /* 检查是否需要扁平化 */
  if (!(flags & OBFUSCATE_CFF)) {
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return 0;  /* 未启用控制流扁平化 */
  }
  
  /* 代码太短不值得扁平化 */
  if (f->sizecode < 4) {
    CFF_LOG("代码太短 (%d 条指令)，跳过扁平化", f->sizecode);
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return 0;
  }
  
  /* 检查是否包含不支持的指令（循环指令） */
  /* 当前简化实现不支持循环指令的扁平化 */
  for (int pc = 0; pc < f->sizecode; pc++) {
    OpCode op = GET_OPCODE(f->code[pc]);
    switch (op) {
      case OP_FORLOOP:
      case OP_FORPREP:
      case OP_TFORPREP:
      case OP_TFORLOOP:
      case OP_TFORCALL:
        /* 包含循环指令，跳过扁平化 */
        CFF_LOG("检测到循环指令 %s @ PC=%d，跳过扁平化", getOpName(op), pc);
        if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
        return 0;
      default:
        break;
    }
  }
  
  /* 初始化上下文 */
  CFFContext *ctx = initContext(L, f, flags, seed);
  if (ctx == NULL) {
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return -1;
  }
  
  /* 识别基本块 */
  if (luaO_identifyBlocks(ctx) != 0) {
    freeContext(ctx);
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return -1;
  }
  
  /* 基本块太少不值得扁平化 */
  if (ctx->num_blocks < 2) {
    CFF_LOG("基本块太少 (%d 个)，跳过扁平化", ctx->num_blocks);
    freeContext(ctx);
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return 0;
  }
  
  /* 如果启用了基本块打乱，进行打乱 */
  if (flags & OBFUSCATE_BLOCK_SHUFFLE) {
    CFF_LOG("启用基本块打乱");
    luaO_shuffleBlocks(ctx);
  }
  
  /* 生成扁平化代码 */
  if (luaO_generateDispatcher(ctx) != 0) {
    CFF_LOG("生成分发器失败！");
    freeContext(ctx);
    if (log_file != NULL) { fclose(log_file); g_cff_log_file = NULL; }
    return -1;
  }
  
  /* 更新函数原型 */
  /* 释放旧代码 */
  luaM_freearray(L, f->code, f->sizecode);
  
  /* 分配新代码 */
  f->code = luaM_newvectorchecked(L, ctx->new_code_size, Instruction);
  memcpy(f->code, ctx->new_code, sizeof(Instruction) * ctx->new_code_size);
  f->sizecode = ctx->new_code_size;
  
  /* 更新栈大小（增加状态寄存器） */
  if (ctx->state_reg >= f->maxstacksize) {
    f->maxstacksize = ctx->state_reg + 1;
  }
  
  /* 在difierline_mode中标记已扁平化 */
  f->difierline_mode |= OBFUSCATE_CFF;
  f->difierline_magicnum = CFF_MAGIC;
  f->difierline_data = ((uint64_t)ctx->num_blocks << 32) | ctx->seed;
  
  CFF_LOG("扁平化完成！新代码大小: %d 条指令", ctx->new_code_size);
  
  freeContext(ctx);
  
  /* 关闭日志文件 */
  if (log_file != NULL) { 
    fclose(log_file); 
    g_cff_log_file = NULL; 
  }
  
  return 0;
}


/*
** 对函数原型进行反扁平化
** @param L Lua状态
** @param f 要处理的函数原型
** @param metadata 扁平化元数据（如果为NULL则从f中读取）
** @return 成功返回0，失败返回错误码
**
** 注意：反扁平化在运行时不是必须的，因为扁平化后的代码
** 仍然是有效的Lua字节码，可以直接执行。
** 这个函数主要用于调试或需要恢复原始控制流的场景。
*/
int luaO_unflatten (lua_State *L, Proto *f, CFFMetadata *metadata) {
  /* 检查是否已扁平化 */
  if (!(f->difierline_mode & OBFUSCATE_CFF)) {
    return 0;  /* 未扁平化，无需处理 */
  }
  
  /* 如果没有提供元数据，从Proto中恢复 */
  if (metadata == NULL) {
    if (f->difierline_magicnum != CFF_MAGIC) {
      return -1;  /* 无效的魔数 */
    }
    
    /* 反扁平化需要保存原始代码信息，当前简化实现不支持完整恢复 */
    /* 这里只清除扁平化标记 */
    f->difierline_mode &= ~OBFUSCATE_CFF;
    return 0;
  }
  
  /* 使用提供的元数据进行反扁平化 */
  /* TODO: 实现完整的反扁平化逻辑 */
  (void)L;
  (void)metadata;
  
  return 0;
}


/*
** 序列化扁平化元数据
** @param L Lua状态
** @param ctx 扁平化上下文
** @param buffer 输出缓冲区
** @param size 缓冲区大小（输入输出参数）
** @return 成功返回0，失败返回错误码
*/
int luaO_serializeMetadata (lua_State *L, CFFContext *ctx, 
                             void *buffer, size_t *size) {
  /* 计算所需大小 */
  size_t needed = sizeof(int) * 4 +  /* magic, version, num_blocks, state_reg */
                  sizeof(unsigned int) +  /* seed */
                  sizeof(BasicBlock) * ctx->num_blocks;
  
  if (buffer == NULL) {
    *size = needed;
    return 0;
  }
  
  if (*size < needed) {
    *size = needed;
    return -1;  /* 缓冲区太小 */
  }
  
  unsigned char *ptr = (unsigned char *)buffer;
  
  /* 写入魔数 */
  *(int *)ptr = CFF_MAGIC;
  ptr += sizeof(int);
  
  /* 写入版本 */
  *(int *)ptr = CFF_VERSION;
  ptr += sizeof(int);
  
  /* 写入基本块数量 */
  *(int *)ptr = ctx->num_blocks;
  ptr += sizeof(int);
  
  /* 写入状态寄存器 */
  *(int *)ptr = ctx->state_reg;
  ptr += sizeof(int);
  
  /* 写入种子 */
  *(unsigned int *)ptr = ctx->seed;
  ptr += sizeof(unsigned int);
  
  /* 写入基本块信息 */
  memcpy(ptr, ctx->blocks, sizeof(BasicBlock) * ctx->num_blocks);
  
  *size = needed;
  
  (void)L;
  return 0;
}


/*
** 反序列化扁平化元数据
** @param L Lua状态
** @param buffer 输入缓冲区
** @param size 缓冲区大小
** @param metadata 输出元数据结构
** @return 成功返回0，失败返回错误码
*/
int luaO_deserializeMetadata (lua_State *L, const void *buffer, 
                               size_t size, CFFMetadata *metadata) {
  if (size < sizeof(int) * 4 + sizeof(unsigned int)) {
    return -1;  /* 数据太短 */
  }
  
  const unsigned char *ptr = (const unsigned char *)buffer;
  
  /* 读取并验证魔数 */
  int magic = *(const int *)ptr;
  if (magic != CFF_MAGIC) {
    return -1;  /* 无效魔数 */
  }
  ptr += sizeof(int);
  
  /* 读取并验证版本 */
  int version = *(const int *)ptr;
  if (version != CFF_VERSION) {
    return -1;  /* 不支持的版本 */
  }
  ptr += sizeof(int);
  
  /* 读取基本块数量 */
  metadata->num_blocks = *(const int *)ptr;
  ptr += sizeof(int);
  
  /* 读取状态寄存器 */
  metadata->state_reg = *(const int *)ptr;
  ptr += sizeof(int);
  
  /* 读取种子 */
  metadata->seed = *(const unsigned int *)ptr;
  ptr += sizeof(unsigned int);
  
  /* 验证数据大小 */
  size_t expected_size = sizeof(int) * 4 + sizeof(unsigned int) +
                         sizeof(BasicBlock) * metadata->num_blocks;
  if (size < expected_size) {
    return -1;  /* 数据不完整 */
  }
  
  /* 分配并读取块映射 */
  metadata->block_mapping = (int *)luaM_malloc_(L, sizeof(int) * metadata->num_blocks, 0);
  if (metadata->block_mapping == NULL) {
    return -1;
  }
  
  /* 从BasicBlock数据提取映射 */
  const BasicBlock *blocks = (const BasicBlock *)ptr;
  for (int i = 0; i < metadata->num_blocks; i++) {
    metadata->block_mapping[i] = blocks[i].start_pc;
  }
  
  metadata->enabled = 1;
  
  return 0;
}


/*
** 释放扁平化元数据占用的内存
** @param L Lua状态
** @param metadata 要释放的元数据
*/
void luaO_freeMetadata (lua_State *L, CFFMetadata *metadata) {
  if (metadata == NULL) return;
  
  if (metadata->block_mapping != NULL) {
    luaM_free_(L, metadata->block_mapping, sizeof(int) * metadata->num_blocks);
    metadata->block_mapping = NULL;
  }
  
  metadata->enabled = 0;
}


/*
** =======================================================
** NOP指令功能实现
** =======================================================
*/


/*
** 生成带虚假参数的NOP指令
** @param seed 随机种子（用于生成随机参数）
** @return 生成的NOP指令
**
** 功能描述：
** 创建一个OP_NOP指令，其A/B/C参数被填充为随机值，
** 这些参数在执行时被忽略，但可以干扰反编译器的分析。
*/
Instruction luaO_createNOP (unsigned int seed) {
  /* 生成随机参数值 */
  unsigned int r = seed;
  NEXT_RAND(r);
  int fake_a = (r >> 16) % 256;   /* 0-255 范围的虚假A参数 */
  NEXT_RAND(r);
  int fake_b = (r >> 16) % 256;   /* 0-255 范围的虚假B参数 */
  NEXT_RAND(r);
  int fake_c = (r >> 16) % 256;   /* 0-255 范围的虚假C参数 */
  
  /* 创建NOP指令：OP_NOP A B C k=0 */
  Instruction nop = CREATE_ABCk(OP_NOP, fake_a, fake_b, fake_c, 0);
  return nop;
}
