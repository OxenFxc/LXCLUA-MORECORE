/*
** ljit_mem.c - 可执行内存管理实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

/* ========================================================================
 * 平台相关内存操作
 * ======================================================================== */

/**
 * @brief 分配可执行内存
 * @param size 大小
 * @return 分配的地址，失败返回NULL
 */
static void *mcode_alloc(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
#endif
}

/**
 * @brief 释放可执行内存
 * @param addr 地址
 * @param size 大小
 */
static void mcode_free(void *addr, size_t size) {
#ifdef _WIN32
    (void)size;
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, size);
#endif
}

/**
 * @brief 设置内存保护
 * @param addr 地址
 * @param size 大小
 * @param exec 是否可执行
 * @param write 是否可写
 * @return 成功返回true
 */
static bool mcode_protect(void *addr, size_t size, bool exec, bool write) {
#ifdef _WIN32
    DWORD old_protect;
    DWORD new_protect;
    
    if (exec && write) {
        new_protect = PAGE_EXECUTE_READWRITE;
    } else if (exec) {
        new_protect = PAGE_EXECUTE_READ;
    } else if (write) {
        new_protect = PAGE_READWRITE;
    } else {
        new_protect = PAGE_READONLY;
    }
    
    return VirtualProtect(addr, size, new_protect, &old_protect) != 0;
#else
    int prot = PROT_READ;
    if (exec) prot |= PROT_EXEC;
    if (write) prot |= PROT_WRITE;
    
    return mprotect(addr, size, prot) == 0;
#endif
}

/**
 * @brief 获取页面大小
 * @return 页面大小
 */
static size_t get_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

/**
 * @brief 对齐到页面边界
 * @param size 大小
 * @return 对齐后的大小
 */
static size_t align_to_page(size_t size) {
    size_t page_size = get_page_size();
    return (size + page_size - 1) & ~(page_size - 1);
}

/* ========================================================================
 * 内存块操作
 * ======================================================================== */

/**
 * @brief 创建新内存块
 * @param size 大小
 * @return 内存块指针，失败返回NULL
 */
static MBlock *mblock_create(size_t size) {
    size = align_to_page(size);
    
    void *addr = mcode_alloc(size);
    if (!addr) return NULL;
    
    MBlock *block = (MBlock *)malloc(sizeof(MBlock));
    if (!block) {
        mcode_free(addr, size);
        return NULL;
    }
    
    block->addr = addr;
    block->size = size;
    block->used = 0;
    block->state = MBLOCK_FREE;
    block->next = NULL;
    
    return block;
}

/**
 * @brief 释放内存块
 * @param block 内存块
 */
static void mblock_free(MBlock *block) {
    if (block) {
        if (block->addr) {
            mcode_free(block->addr, block->size);
        }
        free(block);
    }
}

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
JitError ljit_mem_init(JitMem *mem, size_t initial_size, size_t max_size) {
    memset(mem, 0, sizeof(JitMem));
    
    if (initial_size == 0) initial_size = MCODE_INITIAL_SIZE;
    if (max_size == 0) max_size = MCODE_MAX_SIZE;
    
    mem->max_size = max_size;
    
    /* 创建初始内存块 */
    MBlock *block = mblock_create(initial_size);
    if (!block) return JIT_ERR_MEMORY;
    
    mem->blocks = block;
    mem->current = block;
    mem->total_size = block->size;
    mem->alloc_count = 1;
    
    return JIT_OK;
}

/**
 * @brief 销毁JIT内存管理器
 * @param mem 内存管理器
 */
void ljit_mem_free(JitMem *mem) {
    MBlock *block = mem->blocks;
    while (block) {
        MBlock *next = block->next;
        mblock_free(block);
        block = next;
    }
    memset(mem, 0, sizeof(JitMem));
}

/**
 * @brief 重置内存管理器 (释放所有分配)
 * @param mem 内存管理器
 */
void ljit_mem_reset(JitMem *mem) {
    /* 保留第一个块，释放其他块 */
    MBlock *first = mem->blocks;
    if (!first) return;
    
    MBlock *block = first->next;
    while (block) {
        MBlock *next = block->next;
        mblock_free(block);
        mem->total_size -= block->size;
        block = next;
    }
    
    first->next = NULL;
    first->used = 0;
    first->state = MBLOCK_FREE;
    
    /* 恢复可写权限 */
    mcode_protect(first->addr, first->size, false, true);
    
    mem->current = first;
    mem->total_used = 0;
}

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
JitError ljit_mem_alloc(JitMem *mem, size_t size, void **out_addr) {
    /* 对齐到8字节 */
    size = JIT_ALIGN(size, 8);
    
    MBlock *block = mem->current;
    
    /* 当前块空间不足 */
    if (!block || block->used + size > block->size) {
        /* 检查是否超过最大限制 */
        if (mem->total_size + size > mem->max_size) {
            return JIT_ERR_MEMORY;
        }
        
        /* 分配新块 */
        size_t new_size = JIT_MAX(size, MCODE_INITIAL_SIZE);
        MBlock *new_block = mblock_create(new_size);
        if (!new_block) return JIT_ERR_MEMORY;
        
        new_block->next = mem->blocks;
        mem->blocks = new_block;
        mem->current = new_block;
        mem->total_size += new_block->size;
        mem->alloc_count++;
        
        block = new_block;
    }
    
    /* 分配 */
    *out_addr = (uint8_t *)block->addr + block->used;
    block->used += size;
    block->state = MBLOCK_USED;
    mem->total_used += size;
    
    return JIT_OK;
}

/**
 * @brief 获取当前可写区域
 * @param mem 内存管理器
 * @param min_size 最小需要大小
 * @param out_addr 输出地址
 * @param out_size 输出可用大小
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_reserve(JitMem *mem, size_t min_size,
                           void **out_addr, size_t *out_size) {
    MBlock *block = mem->current;
    
    if (!block || block->size - block->used < min_size) {
        /* 需要新块 */
        if (mem->total_size + min_size > mem->max_size) {
            return JIT_ERR_MEMORY;
        }
        
        size_t new_size = JIT_MAX(min_size, MCODE_INITIAL_SIZE);
        MBlock *new_block = mblock_create(new_size);
        if (!new_block) return JIT_ERR_MEMORY;
        
        new_block->next = mem->blocks;
        mem->blocks = new_block;
        mem->current = new_block;
        mem->total_size += new_block->size;
        mem->alloc_count++;
        
        block = new_block;
    }
    
    *out_addr = (uint8_t *)block->addr + block->used;
    *out_size = block->size - block->used;
    
    return JIT_OK;
}

/**
 * @brief 提交使用的内存
 * @param mem 内存管理器
 * @param used 实际使用大小
 */
void ljit_mem_commit(JitMem *mem, size_t used) {
    MBlock *block = mem->current;
    if (block) {
        used = JIT_ALIGN(used, 8);
        block->used += used;
        block->state = MBLOCK_USED;
        mem->total_used += used;
    }
}

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
JitError ljit_mem_protect_exec(JitMem *mem, void *addr, size_t size) {
    size = align_to_page(size);
    
    if (!mcode_protect(addr, size, true, false)) {
        return JIT_ERR_MEMORY;
    }
    
    /* 更新块状态 */
    for (MBlock *block = mem->blocks; block; block = block->next) {
        if (addr >= block->addr && 
            (uint8_t *)addr < (uint8_t *)block->addr + block->size) {
            block->state = MBLOCK_PROTECTED;
            break;
        }
    }
    
    mem->protect_count++;
    return JIT_OK;
}

/**
 * @brief 设置内存为可写 (不可执行)
 * @param mem 内存管理器
 * @param addr 地址
 * @param size 大小
 * @return 成功返回JIT_OK
 */
JitError ljit_mem_protect_write(JitMem *mem, void *addr, size_t size) {
    size = align_to_page(size);
    
    if (!mcode_protect(addr, size, false, true)) {
        return JIT_ERR_MEMORY;
    }
    
    /* 更新块状态 */
    for (MBlock *block = mem->blocks; block; block = block->next) {
        if (addr >= block->addr && 
            (uint8_t *)addr < (uint8_t *)block->addr + block->size) {
            block->state = MBLOCK_USED;
            break;
        }
    }
    
    return JIT_OK;
}

/**
 * @brief 刷新指令缓存
 * @param addr 地址
 * @param size 大小
 */
void ljit_mem_flush_icache(void *addr, size_t size) {
#ifdef _WIN32
    FlushInstructionCache(GetCurrentProcess(), addr, size);
#elif defined(__GNUC__)
    #if defined(__aarch64__)
        /* ARM64需要显式刷新 */
        uint8_t *p = (uint8_t *)addr;
        uint8_t *end = p + size;
        while (p < end) {
            __asm__ volatile("dc cvau, %0" :: "r"(p));
            p += 64;
        }
        __asm__ volatile("dsb ish");
        p = (uint8_t *)addr;
        while (p < end) {
            __asm__ volatile("ic ivau, %0" :: "r"(p));
            p += 64;
        }
        __asm__ volatile("dsb ish; isb");
    #else
        /* x86/x64不需要显式刷新 */
        (void)addr;
        (void)size;
        __builtin___clear_cache((char *)addr, (char *)addr + size);
    #endif
#else
    (void)addr;
    (void)size;
#endif
}

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
void ljit_mem_stats(JitMem *mem, size_t *total, size_t *used, size_t *available) {
    if (total) *total = mem->total_size;
    if (used) *used = mem->total_used;
    if (available) *available = mem->total_size - mem->total_used;
}

/**
 * @brief 检查地址是否在JIT内存中
 * @param mem 内存管理器
 * @param addr 地址
 * @return 是否在JIT内存中
 */
bool ljit_mem_contains(JitMem *mem, void *addr) {
    for (MBlock *block = mem->blocks; block; block = block->next) {
        if (addr >= block->addr && 
            (uint8_t *)addr < (uint8_t *)block->addr + block->size) {
            return true;
        }
    }
    return false;
}

/* ========================================================================
 * 调试
 * ======================================================================== */

static const char *state_names[] = {
    "FREE", "USED", "PROTECTED"
};

/**
 * @brief 打印内存状态
 * @param mem 内存管理器
 */
void ljit_mem_dump(JitMem *mem) {
    printf("=== JIT Memory Stats ===\n");
    printf("Total: %zu bytes\n", mem->total_size);
    printf("Used: %zu bytes (%.1f%%)\n", mem->total_used,
           mem->total_size > 0 ? 100.0 * mem->total_used / mem->total_size : 0);
    printf("Max: %zu bytes\n", mem->max_size);
    printf("Alloc count: %u\n", mem->alloc_count);
    printf("Protect count: %u\n", mem->protect_count);
    printf("\nBlocks:\n");
    
    int idx = 0;
    for (MBlock *block = mem->blocks; block; block = block->next, idx++) {
        printf("  [%d] addr=%p size=%zu used=%zu state=%s\n",
               idx, block->addr, block->size, block->used,
               state_names[block->state]);
    }
}
