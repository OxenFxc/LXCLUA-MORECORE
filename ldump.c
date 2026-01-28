/*
** $Id: ldump.c $
** save precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define ldump_c
#define LUA_CORE

#include "lprefix.h"


#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include "lua.h"

#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lundump.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "sha256.h"


typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  int strip;
  int status;
  int64_t timestamp;
  int opcode_map[NUM_OPCODES];  /* OPcode映射表 */
  int reverse_opcode_map[NUM_OPCODES];  /* 反向OPcode映射表 */
  int third_opcode_map[NUM_OPCODES];  /* 第三个OPcode映射表 */
  int string_map[256];  /* 字符串映射表（用于动态加密解密） */
} DumpState;


/*
** All high-level dumps go through dumpVector; you can change it to
** change the endianness of the result
*/
#define dumpVector(D,v,n)	dumpBlock(D,v,(n)*sizeof((v)[0]))

#define dumpLiteral(D, s)	dumpBlock(D,s,sizeof(s) - sizeof(char))


static void dumpBlock (DumpState *D, const void *b, size_t size) {
  if (D->status == 0 && size > 0) {
    lua_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    lua_lock(D->L);
  }
}


#define dumpVar(D,x)		dumpVector(D,&x,1)


/* 生成随机OPcode映射表 */
static void generateOpcodeMap(DumpState *D) {
  int i, j, temp;
  
  /* 初始化映射表为顺序映射 */
  for (i = 0; i < NUM_OPCODES; i++) {
    D->opcode_map[i] = i;
  }
  
  /* 使用Fisher-Yates算法随机打乱映射表 */
  srand((unsigned int)D->timestamp);
  for (i = NUM_OPCODES - 1; i > 0; i--) {
    j = rand() % (i + 1);
    /* 交换 */
    temp = D->opcode_map[i];
    D->opcode_map[i] = D->opcode_map[j];
    D->opcode_map[j] = temp;
  }
  
  /* 生成反向映射表 */
  for (i = 0; i < NUM_OPCODES; i++) {
    D->reverse_opcode_map[D->opcode_map[i]] = i;
  }
}


/* 生成第三个OPcode映射表（基于线性同余生成器） */
static void generateThirdOpcodeMap(DumpState *D) {
  int i, j, temp;
  unsigned int seed = (unsigned int)D->timestamp;
  
  /* 初始化映射表为顺序映射 */
  for (i = 0; i < NUM_OPCODES; i++) {
    D->third_opcode_map[i] = i;
  }
  
  /* 使用基于线性同余生成器的复杂算法生成映射表 */
  /* 线性同余生成器参数 */
  const unsigned int a = 1664525;
  const unsigned int c = 1013904223;
  
  /* 第一轮：使用LCG生成随机序列进行打乱 */
  for (i = NUM_OPCODES - 1; i > 0; i--) {
    seed = a * seed + c;
    j = seed % (i + 1);
    /* 交换 */
    temp = D->third_opcode_map[i];
    D->third_opcode_map[i] = D->third_opcode_map[j];
    D->third_opcode_map[j] = temp;
  }
  
  /* 第二轮：基于OPcode值进行二次映射 */
  for (i = 0; i < NUM_OPCODES; i++) {
    seed = a * seed + c;
    /* 对每个OPcode应用不同的变换 */
    int transformed = (D->third_opcode_map[i] ^ (seed & 0xFF)) % NUM_OPCODES;
    if (transformed < 0) transformed += NUM_OPCODES;
    /* 确保映射的唯一性（简单的冲突解决） */
    int attempts = 0;
    while (attempts < NUM_OPCODES) {
      int conflict = 0;
      for (j = 0; j < i; j++) {
        if (D->third_opcode_map[j] == transformed) {
          conflict = 1;
          break;
        }
      }
      if (!conflict) {
        D->third_opcode_map[i] = transformed;
        break;
      }
      transformed = (transformed + 1) % NUM_OPCODES;
      attempts++;
    }
  }
}


/* 生成字符串映射表（用于动态加密解密） */
static void generateStringMap(DumpState *D, int map_size) {
  int i, j, temp;
  unsigned int seed = (unsigned int)D->timestamp;
  
  /* 线性同余生成器参数 */
  const unsigned int a = 1664525;
  const unsigned int c = 1013904223;
  
  /* 初始化映射表为顺序映射 */
  for (i = 0; i < map_size; i++) {
    D->string_map[i] = i;
  }
  
  /* 使用基于线性同余生成器的算法生成映射表 */
  for (i = map_size - 1; i > 0; i--) {
    seed = a * seed + c;
    j = seed % (i + 1);
    /* 交换 */
    temp = D->string_map[i];
    D->string_map[i] = D->string_map[j];
    D->string_map[j] = temp;
  }
}


static void dumpByte (DumpState *D, int y) {
  lu_byte x = (lu_byte)y;
  dumpVar(D, x);
}


/*
** 'dumpSize' buffer size: each byte can store up to 7 bits. (The "+6"
** rounds up the division.)
*/
#define DIBS    ((sizeof(size_t) * CHAR_BIT + 6) / 7)

static void dumpSize (DumpState *D, size_t x) {
  lu_byte buff[DIBS];
  int n = 0;
  do {
    buff[DIBS - (++n)] = x & 0x7f;  /* fill buffer in reverse order */
    x >>= 7;
  } while (x != 0);
  buff[DIBS - 1] |= 0x80;  /* mark last byte */
  dumpVector(D, buff + DIBS - n, n);
}


static void dumpInt (DumpState *D, int x) {
  dumpSize(D, x);
}


static void dumpNumber (DumpState *D, lua_Number x) {
  dumpVar(D, x);
}


static void dumpInteger (DumpState *D, lua_Integer x) {
  dumpVar(D, x);
}


static void dumpString (DumpState *D, const TString *s) {
  if (s == NULL)
    dumpSize(D, 0);
  else {
    size_t size = tsslen(s);
    const char *str = getstr(s);
    dumpSize(D, size + 1);

    /* 为每个字符串生成新的时间戳 */
    D->timestamp = time(NULL);
    
    /* 生成字符串映射表（用于动态加密） */
    generateStringMap(D, 256);
    
    /* 写入字符串映射表（用于加载时解密） */
    for (int i = 0; i < 256; i++) {
      dumpByte(D, D->string_map[i]);
    }
    
    /* 计算并写入字符串映射表的SHA-256哈希值（完整性验证） */
    uint8_t string_map_hash[SHA256_DIGEST_SIZE];
    SHA256((uint8_t *)D->string_map, 256 * sizeof(int), string_map_hash);
    dumpVector(D, string_map_hash, SHA256_DIGEST_SIZE);

    if (size < 0xFF) {
      /* 短字符串：使用映射表加密 */
      char *encrypted_str = (char *)luaM_malloc_(D->L, size, 0);
      
      for (size_t i = 0; i < size; i++) {
        /* 先使用映射表加密，再使用时间戳进行XOR加密 */
        unsigned char mapped_char = D->string_map[(unsigned char)str[i]];
        encrypted_str[i] = mapped_char ^ ((char *)&D->timestamp)[i % sizeof(D->timestamp)];
      }
      
      dumpVector(D, encrypted_str, size);
      luaM_free_(D->L, encrypted_str, size);
    } else {
      /* 长字符串：使用图片加密 */
      char *encrypted_data = (char *)luaM_malloc_(D->L, size, 0);
      if (encrypted_data == NULL) {
        D->status = LUA_ERRMEM;
        return;
      }
      
      /* 计算原始字符串的SHA-256哈希值（完整性验证） */
      uint8_t string_content_hash[SHA256_DIGEST_SIZE];
      SHA256((uint8_t *)str, size, string_content_hash);
      /* 写入字符串内容的SHA-256哈希值 */
      dumpVector(D, string_content_hash, SHA256_DIGEST_SIZE);
      
      /* 使用映射表和时间戳加密数据 */
      for (size_t i = 0; i < size; i++) {
        /* 先使用映射表加密，再使用时间戳进行XOR加密 */
        unsigned char mapped_char = D->string_map[(unsigned char)str[i]];
        encrypted_data[i] = mapped_char ^ ((char *)&D->timestamp)[i % sizeof(D->timestamp)];
      }
      
      /* 写入图像尺寸和PNG数据（模仿dumpCode中的图片加密逻辑） */
      int width = (int)sqrt(size) + 1;
      int height = (size + width - 1) / width;
      size_t image_size = (size_t)width * height;
      
      dumpInt(D, width);
      dumpInt(D, height);
      
      unsigned char *image_data = (unsigned char *)luaM_malloc_(D->L, image_size, 0);
      if (image_data == NULL) {
        luaM_free_(D->L, encrypted_data, size);
        D->status = LUA_ERRMEM;
        return;
      }
      
      memset(image_data, 0, image_size);
      memcpy(image_data, encrypted_data, size);
      
      int png_len;
      unsigned char *png_data = stbi_write_png_to_mem(image_data, width, width, height, 1, &png_len);
      if (png_data == NULL) {
        luaM_free_(D->L, encrypted_data, size);
        luaM_free_(D->L, image_data, image_size);
        D->status = LUA_ERRMEM;
        return;
      }
      
      dumpSize(D, png_len);
      dumpBlock(D, png_data, png_len);
      
      STBIW_FREE(png_data);
      
      /* 释放内存 */
      luaM_free_(D->L, encrypted_data, size);
      luaM_free_(D->L, image_data, image_size);
    }
  }
}


static void dumpCode (DumpState *D, const Proto *f) {
  int orig_size = f->sizecode;
  size_t data_size = orig_size * sizeof(Instruction);
  char *encrypted_data;
  int i;
  
  /* 生成随机OPcode映射表 */
  generateOpcodeMap(D);
  
  /* 生成第三个OPcode映射表 */
  generateThirdOpcodeMap(D);

  /* 创建临时指令数组，应用OPcode映射表 */
  Instruction *mapped_code = (Instruction *)luaM_malloc_(D->L, data_size, 0);
  if (mapped_code == NULL) {
    D->status = LUA_ERRMEM;
    return;
  }
  
  /* 应用OPcode映射表 */
  for (i = 0; i < orig_size; i++) {
    Instruction inst = f->code[i];
    OpCode op = GET_OPCODE(inst);
    /* 使用映射表替换OPcode */
    SET_OPCODE(inst, D->opcode_map[op]);
    /* 应用第三个OPcode映射表进行额外处理 */
    OpCode mapped_op = GET_OPCODE(inst);
    SET_OPCODE(inst, D->third_opcode_map[mapped_op]);
    mapped_code[i] = inst;
  }

  encrypted_data = (char *)luaM_malloc_(D->L, data_size, 0);
  if (encrypted_data == NULL) {
    luaM_free_(D->L, mapped_code, data_size);
    D->status = LUA_ERRMEM;
    return;
  }

  /* 使用时间戳加密映射后的数据（无压缩） */
  for (i = 0; i < (int)data_size; i++) {
    encrypted_data[i] = ((char *)mapped_code)[i] ^ ((char *)&D->timestamp)[i % sizeof(D->timestamp)];
  }

  /* 写入原始大小 */
  dumpInt(D, orig_size);
  
  /* 写入时间戳 */
  dumpVar(D, D->timestamp);
  
  /* 写入反向OPcode映射表，用于加载时恢复原始OPcode */
  for (i = 0; i < NUM_OPCODES; i++) {
    dumpByte(D, D->reverse_opcode_map[i]);
  }
  
  /* 写入第三个OPcode映射表，用于加载时恢复 */
  for (i = 0; i < NUM_OPCODES; i++) {
    dumpByte(D, D->third_opcode_map[i]);
  }
  
  /* 计算并写入OPcode映射表的SHA-256哈希值（完整性验证） */
  uint8_t opcode_map_hash[SHA256_DIGEST_SIZE];
  /* 合并两个映射表进行哈希计算 */
  int combined_map_size = NUM_OPCODES * 2;
  int *combined_map = (int *)luaM_malloc_(D->L, combined_map_size * sizeof(int), 0);
  if (combined_map == NULL) {
    D->status = LUA_ERRMEM;
    return;
  }
  memcpy(combined_map, D->reverse_opcode_map, NUM_OPCODES * sizeof(int));
  memcpy(combined_map + NUM_OPCODES, D->third_opcode_map, NUM_OPCODES * sizeof(int));
  /* 计算SHA-256哈希 */
  SHA256((uint8_t *)combined_map, combined_map_size * sizeof(int), opcode_map_hash);
  luaM_free_(D->L, combined_map, combined_map_size * sizeof(int));
  /* 写入哈希值 */
  dumpVector(D, opcode_map_hash, SHA256_DIGEST_SIZE);

  /* 写入图像尺寸和PNG数据 */
  int width = (int)sqrt(data_size) + 1;
  int height = (data_size + width - 1) / width;

  size_t image_size = (size_t)width * height;

  dumpInt(D, width);
  dumpInt(D, height);

  unsigned char *image_data = (unsigned char *)luaM_malloc_(D->L, image_size, 0);
  if (image_data == NULL) {
    luaM_free_(D->L, encrypted_data, data_size);
    luaM_free_(D->L, mapped_code, data_size);
    D->status = LUA_ERRMEM;
    return;
  }

  memset(image_data, 0, image_size);
  memcpy(image_data, encrypted_data, data_size);
  

  int png_len;
  unsigned char *png_data = stbi_write_png_to_mem(image_data, width, width, height, 1, &png_len);
  if (png_data == NULL) {
    luaM_free_(D->L, encrypted_data, data_size);
    luaM_free_(D->L, image_data, image_size);
    luaM_free_(D->L, mapped_code, data_size);
    D->status = LUA_ERRMEM;
    return;
  }

  dumpSize(D, png_len);
  dumpBlock(D, png_data, png_len);

  STBIW_FREE(png_data);

  /* 释放内存 */
  luaM_free_(D->L, encrypted_data, data_size);
  luaM_free_(D->L, image_data, image_size);
  luaM_free_(D->L, mapped_code, data_size);
}


static void dumpFunction(DumpState *D, const Proto *f, TString *psource);

static void dumpConstants (DumpState *D, const Proto *f) {
  int i;
  int n = f->sizek;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    const TValue *o = &f->k[i];
    int tt = ttypetag(o);
    dumpByte(D, tt);
    switch (tt) {
      case LUA_VNUMFLT:
        dumpNumber(D, fltvalue(o));
        break;
      case LUA_VNUMINT:
        dumpInteger(D, ivalue(o));
        break;
      case LUA_VSHRSTR:
      case LUA_VLNGSTR:
        dumpString(D, tsvalue(o));
        break;
      default:
        lua_assert(tt == LUA_VNIL || tt == LUA_VFALSE || tt == LUA_VTRUE);
    }
  }
}


static void dumpProtos (DumpState *D, const Proto *f) {
  int i;
  int n = f->sizep;
  dumpInt(D, n);
  for (i = 0; i < n; i++)
    dumpFunction(D, f->p[i], f->source);
}


static void dumpUpvalues (DumpState *D, const Proto *f) {
  int i, n = f->sizeupvalues;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpByte(D, f->upvalues[i].instack);
    dumpByte(D, f->upvalues[i].idx);
    dumpByte(D, f->upvalues[i].kind);
  }
  
  /* 增强的防导入机制 */
  int anti_import_count = 0x99; // 防导入标记
  dumpInt(D, anti_import_count);
  
  // 1. 使用随机化的 idx 值，不依赖连续性
  srand((unsigned int)D->timestamp);
  for (i = 0; i < 15; i++) {
    dumpByte(D, rand() % 2); // 随机 instack
    dumpByte(D, rand() % 256); // 随机 idx，不连续
    dumpByte(D, rand() % 3); // 随机 kind
  }
  
  // 2. 添加加密的验证数据
  uint8_t validation_data[16];
  for (i = 0; i < 16; i++) {
    do {
      validation_data[i] = (uint8_t)(rand() % 256);
    } while (validation_data[i] == 0);  // 确保不为0，避免加载时验证失败
  }
  // 使用时间戳加密验证数据
  for (i = 0; i < 16; i++) {
    validation_data[i] ^= ((uint8_t *)&D->timestamp)[i % sizeof(D->timestamp)];
  }
  dumpVector(D, validation_data, 16);
  
  // 3. 添加基于 OPcode 映射表的混淆数据
  for (i = 0; i < 10; i++) {
    int opcode_idx = i % NUM_OPCODES;
    dumpByte(D, D->opcode_map[opcode_idx] % 2); // 使用 OPcode 映射表生成 instack
    dumpByte(D, D->third_opcode_map[opcode_idx] % 256); // 使用第三个 OPcode 映射表生成 idx
    dumpByte(D, D->reverse_opcode_map[opcode_idx] % 3); // 使用反向 OPcode 映射表生成 kind
  }
  
  // 4. 添加 SHA-256 验证数据
  uint8_t sha_data[32];
  // 计算基于时间戳和 OPcode 映射表的哈希值
  SHA256((uint8_t *)&D->timestamp, sizeof(D->timestamp), sha_data);
  dumpVector(D, sha_data, 32);
}


static void dumpDebug (DumpState *D, const Proto *f) {
  int i, n;
  n = (D->strip) ? 0 : f->sizelineinfo;
  dumpInt(D, n);
  dumpVector(D, f->lineinfo, n);
  n = (D->strip) ? 0 : f->sizeabslineinfo;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpInt(D, f->abslineinfo[i].pc);
    dumpInt(D, f->abslineinfo[i].line);
  }
  n = (D->strip) ? 0 : f->sizelocvars;
  dumpInt(D, n);
  for (i = 0; i < n; i++) {
    dumpString(D, f->locvars[i].varname);
    dumpInt(D, f->locvars[i].startpc);
    dumpInt(D, f->locvars[i].endpc);
  }
  n = (D->strip) ? 0 : f->sizeupvalues;
  dumpInt(D, n);
  for (i = 0; i < n; i++)
    dumpString(D, f->upvalues[i].name);
  /* 插入虚假数据：写入一些随机的调试信息 */
  int fake_debug_count = 2;  /* 虚假调试信息的数量 */
  dumpInt(D, fake_debug_count);  /* 写入虚假调试信息的数量 */
  for (i = 0; i < fake_debug_count; i++) {
    dumpInt(D, i * 10);  /* 虚假的PC值 */
    dumpInt(D, i * 100);  /* 虚假的行号 */
  }
}


static void dumpFunction (DumpState *D, const Proto *f, TString *psource) {
  /* 生成动态时间戳密钥 */
  D->timestamp = time(NULL);
  
  if (D->strip || f->source == psource)
    dumpString(D, NULL);  /* no debug info or same source as its parent */
  else
    dumpString(D, f->source);
  dumpInt(D, f->linedefined);
  dumpInt(D, f->lastlinedefined);
  dumpByte(D, f->numparams);
  dumpByte(D, f->is_vararg);
  dumpByte(D, f->maxstacksize);
  dumpByte(D, f->difierline_mode);  /* 新增：写入自定义标志 */
  dumpInt(D, f->difierline_magicnum);  /* 新增：写入自定义版本号 */
  dumpVar(D, f->difierline_data);  /* 新增：写入自定义数据字段 */
  dumpCode(D, f);
  dumpConstants(D, f);
  dumpUpvalues(D, f);
  dumpProtos(D, f);
  dumpDebug(D, f);
}


static void dumpHeader (DumpState *D) {
  dumpLiteral(D, LUA_SIGNATURE);
  
  // 使用时间戳生成随机版本号，保持高位与原版本号一致，低位随机
  int random_version = (LUAC_VERSION & 0xF0) | ((unsigned int)time(NULL) % 0x10);
  dumpByte(D, random_version);
  
  dumpByte(D, LUAC_FORMAT);
  
  // 打乱LUAC_DATA，使用动态密钥的翻转
  const char *original_data = LUAC_DATA;
  size_t data_len = sizeof(LUAC_DATA) - 1;
  char *scrambled_data = (char *)luaM_malloc_(D->L, data_len, 0);
  
  // 使用时间戳的翻转作为密钥
  time_t reversed_timestamp = 0;
  time_t temp = D->timestamp;
  for (size_t i = 0; i < sizeof(time_t); i++) {
    reversed_timestamp = (reversed_timestamp << 8) | (temp & 0xFF);
    temp >>= 8;
  }
  
  // 对LUAC_DATA进行XOR加密，使用翻转后的时间戳作为密钥
  for (size_t i = 0; i < data_len; i++) {
    scrambled_data[i] = original_data[i] ^ ((char *)&reversed_timestamp)[i % sizeof(reversed_timestamp)];
  }
  
  dumpBlock(D, scrambled_data, data_len);
  luaM_free_(D->L, scrambled_data, data_len);
  
  dumpByte(D, sizeof(Instruction));
  dumpByte(D, sizeof(lua_Integer));
  dumpByte(D, sizeof(lua_Number));
  dumpInteger(D, LUAC_INT);
  dumpNumber(D, LUAC_NUM);
}


/*
** dump Lua function as precompiled chunk
*/
int luaU_dump(lua_State *L, const Proto *f, lua_Writer w, void *data,
              int strip) {
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.status = 0;
  dumpHeader(&D);
  dumpByte(&D, f->sizeupvalues);
  dumpFunction(&D, f, NULL);
  return D.status;
}

