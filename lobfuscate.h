/*
** $Id: lobfuscate.h $
** Control Flow Flattening Obfuscation for Lua bytecode
** DifierLine
*/

#ifndef lobfuscate_h
#define lobfuscate_h

#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"

/*
** =======================================================
** 控制流扁平化混淆模块
** =======================================================
** 
** 控制流扁平化（Control Flow Flattening）是一种代码混淆技术，
** 其核心思想是将原始的控制流结构（如顺序执行、条件分支、循环等）
** 转换为一个统一的dispatcher-switch结构，使得静态分析变得困难。
**
** 原理：
** 1. 将代码划分为基本块（Basic Blocks）
** 2. 为每个基本块分配一个唯一的状态ID
** 3. 添加一个dispatcher循环，根据状态变量跳转到对应的基本块
** 4. 将原始的跳转指令转换为状态变量赋值
**
** 变换前:
**   block1:
**     ...
**     if cond then goto block2 else goto block3
**   block2:
**     ...
**     goto block4
**   block3:
**     ...
**     goto block4
**   block4:
**     ...
**
** 变换后:
**   state = initial_state
**   while true do
**     switch(state) {
**       case 1: ... state = cond ? 2 : 3; break;
**       case 2: ... state = 4; break;
**       case 3: ... state = 4; break;
**       case 4: ... return; break;
**     }
**   end
*/


/*
** 混淆模式标志位（可组合使用）
*/
#define OBFUSCATE_NONE          0       /* 不进行混淆 */
#define OBFUSCATE_CFF           (1<<0)  /* 控制流扁平化 */
#define OBFUSCATE_BLOCK_SHUFFLE (1<<1)  /* 基本块随机打乱顺序 */
#define OBFUSCATE_BOGUS_BLOCKS  (1<<2)  /* 插入虚假基本块 */
#define OBFUSCATE_STATE_ENCODE  (1<<3)  /* 状态值编码混淆 */


/*
** 基本块结构体
** 用于在扁平化过程中表示一个代码基本块
*/
typedef struct BasicBlock {
  int start_pc;           /* 基本块起始PC */
  int end_pc;             /* 基本块结束PC（不包含） */
  int state_id;           /* 分配的状态ID */
  int original_target;    /* 原始跳转目标（如果是跳转块） */
  int fall_through;       /* 顺序执行的下一个块ID（-1表示无） */
  int cond_target;        /* 条件跳转目标块ID（-1表示无） */
  int is_entry;           /* 是否为入口块 */
  int is_exit;            /* 是否为出口块（包含RETURN指令） */
} BasicBlock;


/*
** 扁平化上下文结构体
** 保存扁平化过程中的所有状态信息
*/
typedef struct CFFContext {
  lua_State *L;             /* Lua状态 */
  Proto *f;                 /* 原始函数原型 */
  BasicBlock *blocks;       /* 基本块数组 */
  int num_blocks;           /* 基本块数量 */
  int block_capacity;       /* 基本块数组容量 */
  Instruction *new_code;    /* 新生成的代码 */
  int new_code_size;        /* 新代码大小 */
  int new_code_capacity;    /* 新代码容量 */
  int state_reg;            /* 状态变量寄存器 */
  int dispatcher_pc;        /* dispatcher位置 */
  unsigned int seed;        /* 随机数种子 */
  int obfuscate_flags;      /* 混淆标志 */
} CFFContext;


/*
** 扁平化元数据结构体
** 保存用于解扁平化的元数据信息
*/
typedef struct CFFMetadata {
  int enabled;              /* 是否启用了扁平化 */
  int num_blocks;           /* 基本块数量 */
  int state_reg;            /* 状态变量寄存器 */
  int dispatcher_pc;        /* dispatcher位置 */
  int *block_mapping;       /* 状态ID到原始PC的映射 */
  int original_size;        /* 原始代码大小 */
  unsigned int seed;        /* 用于恢复的随机种子 */
} CFFMetadata;


/*
** =======================================================
** 公共API函数声明
** =======================================================
*/


/*
** 对函数原型进行控制流扁平化
** @param L Lua状态
** @param f 要处理的函数原型
** @param flags 混淆标志位组合
** @param seed 随机种子（用于可重复的混淆）
** @return 成功返回0，失败返回错误码
** 
** 功能描述：
** - 分析字节码，识别基本块边界
** - 构建控制流图
** - 生成扁平化后的代码
** - 更新函数原型中的代码
** - 如果提供了log_path，输出详细的转换日志到文件
*/
LUAI_FUNC int luaO_flatten (lua_State *L, Proto *f, int flags, unsigned int seed,
                            const char *log_path);


/*
** 对函数原型进行反扁平化（恢复原始控制流）
** @param L Lua状态
** @param f 要处理的函数原型
** @param metadata 扁平化元数据
** @return 成功返回0，失败返回错误码
**
** 功能描述：
** - 根据元数据恢复原始的基本块结构
** - 重建原始的跳转指令
** - 移除dispatcher代码
*/
LUAI_FUNC int luaO_unflatten (lua_State *L, Proto *f, CFFMetadata *metadata);


/*
** 序列化扁平化元数据（用于保存到字节码文件）
** @param L Lua状态
** @param ctx 扁平化上下文
** @param buffer 输出缓冲区
** @param size 缓冲区大小（输入输出参数）
** @return 成功返回0，失败返回错误码
**
** 功能描述：
** - 将扁平化相关的元数据序列化为二进制格式
** - 包括基本块映射、状态寄存器等信息
*/
LUAI_FUNC int luaO_serializeMetadata (lua_State *L, CFFContext *ctx, 
                                       void *buffer, size_t *size);


/*
** 反序列化扁平化元数据（从字节码文件加载）
** @param L Lua状态
** @param buffer 输入缓冲区
** @param size 缓冲区大小
** @param metadata 输出元数据结构
** @return 成功返回0，失败返回错误码
**
** 功能描述：
** - 从二进制数据恢复扁平化元数据
** - 验证数据完整性
*/
LUAI_FUNC int luaO_deserializeMetadata (lua_State *L, const void *buffer, 
                                         size_t size, CFFMetadata *metadata);


/*
** 释放扁平化元数据占用的内存
** @param L Lua状态
** @param metadata 要释放的元数据
*/
LUAI_FUNC void luaO_freeMetadata (lua_State *L, CFFMetadata *metadata);


/*
** =======================================================
** 内部辅助函数声明
** =======================================================
*/


/*
** 识别并构建基本块
** @param ctx 扁平化上下文
** @return 成功返回0，失败返回错误码
*/
LUAI_FUNC int luaO_identifyBlocks (CFFContext *ctx);


/*
** 检查指令是否为基本块终结指令
** @param op 操作码
** @return 是终结指令返回1，否则返回0
*/
LUAI_FUNC int luaO_isBlockTerminator (OpCode op);


/*
** 检查指令是否为跳转指令
** @param op 操作码
** @return 是跳转指令返回1，否则返回0
*/
LUAI_FUNC int luaO_isJumpInstruction (OpCode op);


/*
** 获取跳转指令的目标PC
** @param inst 指令
** @param pc 当前PC
** @return 跳转目标PC
*/
LUAI_FUNC int luaO_getJumpTarget (Instruction inst, int pc);


/*
** 生成dispatcher代码
** @param ctx 扁平化上下文
** @return 成功返回0，失败返回错误码
*/
LUAI_FUNC int luaO_generateDispatcher (CFFContext *ctx);


/*
** 随机打乱基本块顺序
** @param ctx 扁平化上下文
*/
LUAI_FUNC void luaO_shuffleBlocks (CFFContext *ctx);


/*
** 编码状态值（增加混淆程度）
** @param state 原始状态值
** @param seed 随机种子
** @return 编码后的状态值
*/
LUAI_FUNC int luaO_encodeState (int state, unsigned int seed);


/*
** 解码状态值
** @param encoded_state 编码后的状态值
** @param seed 随机种子
** @return 原始状态值
*/
LUAI_FUNC int luaO_decodeState (int encoded_state, unsigned int seed);


/*
** 生成带虚假参数的NOP指令
** @param seed 随机种子（用于生成随机参数）
** @return 生成的NOP指令
*/
LUAI_FUNC Instruction luaO_createNOP (unsigned int seed);


#endif /* lobfuscate_h */
