/*
** ljit_mem.h - 可执行内存管理
** LXCLUA-NCore JIT Module
*/

#ifndef ljit_mem_h
#define ljit_mem_h

#include "../ljit_types.h"

/* ========================================================================
 * 内存区域配置
 * ======================================================================== */

#define MCODE_PAGE_SIZE     4096
#define MCODE_INITIAL_SIZE  (64 * 1024)      /**< 64KB初始 */
#define MCODE_MAX_SIZE      (64 * 1024 * 1024) /**< 64MB最大 */

/* ========================================================================
 * 内存块状态
 * ======================================================================== */

/**
 * @brief 内存块状态
 */
typedef enum {
    MBLOCK_FREE = 0,         /**< 空闲 */
    MBLOCK_USED,             /**< 已使用 */
    MBLOCK_PROTECTED         /**< 写保护 */
} MBlockState;

/**
 * @brief 内存块
 */
typedef struct MBlock {
    void *addr;              /**< 起始地址 */
    size_t size;             /**< 块大小 */
    size_t used;             /**< 已使用大小 */
    MBlockState state;
    struct MBlock *next;     /**< 下一块 */
} MBlock;

/**
 * @brief JIT内存管理器
 */
typedef struct JitMem {
    MBlock *blocks;          /**< 内存块链表 */
    MBlock *current;         /**< 当前分配块 */
    
    size_t total_size;       /**< 总分配大小 */
    size_t total_used;       /**< 总使用大小 */
    size_t max_size;         /**< 最大允许大小 */
    
    /* 统计 */
    uint32_t alloc_count;
    uint32_t free_count;
    uint32_t protect_count;
    
} JitMem;

/* ========================================================================
 * 内存管理器生命周期
 * ======================================================================== */

/**
 * @brief 初始化JIT内存管理器
 * @param mem 内存管理器
 * @param initial_size 初始大小 (0使用默认值)
 * @param max_size 最大大小 (0使用默认值)
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_init(JitMem *mem, size_t initial_size, size_t max_size);

/**
 * @brief 销毁JIT内存管理器
 * @param mem 内存管理器
 */
void ljit_mem_free(JitMem *mem);

/**
 * @brief 重置内存管理器 (释放所有分配)
 * @param mem 内存管理器
 */
void ljit_mem_reset(JitMem *mem);

/* ========================================================================
 * 内存分配
 * ======================================================================== */

/**
 * @brief 分配可写内存
 * @param mem 内存管理器
 * @param size 请求大小
 * @param out_addr 输出地址
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_alloc(JitMem *mem, size_t size, void **out_addr);

/**
 * @brief 获取当前可写区域
 * @param mem 内存管理器
 * @param min_size 最小需要大小
 * @param out_addr 输出地址
 * @param out_size 输出可用大小
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_reserve(JitMem *mem, size_t min_size, 
                           void **out_addr, size_t *out_size);

/**
 * @brief 提交使用的内存
 * @param mem 内存管理器
 * @param used 实际使用大小
 */
void ljit_mem_commit(JitMem *mem, size_t used);

/* ========================================================================
 * 内存保护
 * ======================================================================== */

/**
 * @brief 设置内存为可执行 (不可写)
 * @param mem 内存管理器
 * @param addr 地址
 * @param size 大小
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_protect_exec(JitMem *mem, void *addr, size_t size);

/**
 * @brief 设置内存为可写 (不可执行)
 * @param mem 内存管理器
 * @param addr 地址
 * @param size 大小
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_protect_write(JitMem *mem, void *addr, size_t size);

/**
 * @brief 刷新指令缓存
 * @param addr 地址
 * @param size 大小
 */
void ljit_mem_flush_icache(void *addr, size_t size);

/* ========================================================================
 * 查询
 * ======================================================================== */

/**
 * @brief 获取内存统计
 * @param mem 内存管理器
 * @param total 输出总大小
 * @param used 输出已使用大小
 * @param available 输出可用大小
 */
void ljit_mem_stats(JitMem *mem, size_t *total, size_t *used, size_t *available);

/**
 * @brief 检查地址是否在JIT内存中
 * @param mem 内存管理器
 * @param addr 地址
 * @return 是否在JIT内存中
 */
bool ljit_mem_contains(JitMem *mem, void *addr);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印内存状态
 * @param mem 内存管理器
 */
void ljit_mem_dump(JitMem *mem);

#endif /* ljit_mem_h */
