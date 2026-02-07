/*
** ljit_core.c - JIT编译器主控制器实现
** LXCLUA-NCore JIT Module
*/

#include "ljit_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 默认配置
 * ======================================================================== */

#define DEFAULT_MAXMCODE    (64 * 1024 * 1024)  /* 64MB */
#define DEFAULT_MAXTRACE    1000
#define DEFAULT_MAXRECORD   4000
#define DEFAULT_MAXIRCONST  4096
#define DEFAULT_MAXSIDE     100
#define DEFAULT_MAXSNAP     500

/* ========================================================================
 * 配置管理
 * ======================================================================== */

/**
 * @brief 设置默认配置
 * @param config 配置结构
 */
void ljit_config_default(JitConfig *config) {
    memset(config, 0, sizeof(JitConfig));
    
    config->enable = true;
    config->enable_opt = true;
    
    config->hotloop = JIT_HOTLOOP_DEFAULT;
    config->hotcall = JIT_HOTCALL_DEFAULT;
    config->hotexit = JIT_HOTSIDE_DEFAULT;
    
    config->maxmcode = DEFAULT_MAXMCODE;
    config->maxtrace = DEFAULT_MAXTRACE;
    config->maxrecord = DEFAULT_MAXRECORD;
    config->maxirconst = DEFAULT_MAXIRCONST;
    config->maxside = DEFAULT_MAXSIDE;
    config->maxsnap = DEFAULT_MAXSNAP;
    
    ljit_opt_config_default(&config->opt);
}

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

/**
 * @brief 初始化JIT引擎
 * @param jit JIT引擎
 * @param L Lua状态
 * @return 成功返回JIT_OK
 */
JitError ljit_init(JitEngine *jit, void *L) {
    memset(jit, 0, sizeof(JitEngine));
    jit->L = L;
    
    /* 设置默认配置 */
    ljit_config_default(&jit->config);
    
    /* 初始化上下文 */
    jit->ctx.state = JIT_STATE_IDLE;
    jit->ctx.hotcount.loop_threshold = jit->config.hotloop;
    jit->ctx.hotcount.call_threshold = jit->config.hotcall;
    jit->ctx.hotcount.side_threshold = jit->config.hotexit;
    
    /* 初始化内存管理 */
    JitError err = ljit_mem_init(&jit->mem, 0, jit->config.maxmcode);
    if (err != JIT_OK) return err;
    
    jit->ctx.mem = &jit->mem;
    
    /* 初始化热点计数 */
    ljit_hotcount_init(&jit->hotcounts);
    ljit_hotcount_set_thresh(&jit->hotcounts,
                              jit->config.hotloop,
                              jit->config.hotcall,
                              jit->config.hotexit);
    
    /* 初始化记录器 */
    err = ljit_rec_init(&jit->recorder, &jit->ctx);
    if (err != JIT_OK) {
        ljit_mem_free(&jit->mem);
        return err;
    }
    
    return JIT_OK;
}

/**
 * @brief 销毁JIT引擎
 * @param jit JIT引擎
 */
void ljit_free(JitEngine *jit) {
    /* 释放所有Trace */
    if (jit->ctx.traces) {
        for (uint32_t i = 0; i < jit->ctx.trace_count; i++) {
            Trace *t = jit->ctx.traces[i];
            if (t) {
                if (t->ir) free(t->ir);
                if (t->consts) free(t->consts);
                if (t->exits) free(t->exits);
                free(t);
            }
        }
        free(jit->ctx.traces);
    }
    
    ljit_rec_free(&jit->recorder);
    ljit_mem_free(&jit->mem);
    
    memset(jit, 0, sizeof(JitEngine));
}

/**
 * @brief 重置JIT引擎
 * @param jit JIT引擎
 */
void ljit_reset(JitEngine *jit) {
    /* 释放所有Trace */
    if (jit->ctx.traces) {
        for (uint32_t i = 0; i < jit->ctx.trace_count; i++) {
            Trace *t = jit->ctx.traces[i];
            if (t) {
                if (t->ir) free(t->ir);
                if (t->consts) free(t->consts);
                if (t->exits) free(t->exits);
                free(t);
            }
        }
        jit->ctx.trace_count = 0;
    }
    
    ljit_mem_reset(&jit->mem);
    ljit_hotcount_reset(&jit->hotcounts);
    ljit_rec_reset(&jit->recorder);
    
    jit->ctx.state = JIT_STATE_IDLE;
    jit->ctx.cur_trace_id = 0;
    
    memset(&jit->stats, 0, sizeof(jit->stats));
}

/**
 * @brief 应用配置
 * @param jit JIT引擎
 * @param config 配置
 * @return 成功返回JIT_OK
 */
JitError ljit_config_apply(JitEngine *jit, const JitConfig *config) {
    jit->config = *config;
    
    jit->ctx.hotcount.loop_threshold = config->hotloop;
    jit->ctx.hotcount.call_threshold = config->hotcall;
    jit->ctx.hotcount.side_threshold = config->hotexit;
    
    ljit_hotcount_set_thresh(&jit->hotcounts,
                              config->hotloop,
                              config->hotcall,
                              config->hotexit);
    
    return JIT_OK;
}

/* ========================================================================
 * JIT控制
 * ======================================================================== */

/**
 * @brief 启用JIT
 * @param jit JIT引擎
 */
void ljit_enable(JitEngine *jit) {
    jit->config.enable = true;
}

/**
 * @brief 禁用JIT
 * @param jit JIT引擎
 */
void ljit_disable(JitEngine *jit) {
    jit->config.enable = false;
}

/**
 * @brief 检查JIT是否启用
 * @param jit JIT引擎
 * @return 是否启用
 */
bool ljit_is_enabled(JitEngine *jit) {
    return jit->config.enable;
}

/**
 * @brief 刷新所有编译结果
 * @param jit JIT引擎
 */
void ljit_flush(JitEngine *jit) {
    ljit_reset(jit);
}

/* ========================================================================
 * 热点检测
 * ======================================================================== */

/**
 * @brief 检查循环热点
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 是否需要编译
 */
bool ljit_check_hotloop(JitEngine *jit, Proto *proto, uint32_t pc) {
    if (!jit->config.enable) return false;
    if (jit->ctx.state != JIT_STATE_IDLE) return false;
    
    /* 检查是否已有编译结果 */
    Trace *existing = ljit_find_trace(jit, proto, pc);
    if (existing) return true;
    
    /* 检查热点计数 */
    return ljit_hotcount_loop(&jit->hotcounts, pc);
}

/**
 * @brief 检查调用热点
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 是否需要编译
 */
bool ljit_check_hotcall(JitEngine *jit, Proto *proto, uint32_t pc) {
    if (!jit->config.enable) return false;
    if (jit->ctx.state != JIT_STATE_IDLE) return false;
    
    Trace *existing = ljit_find_trace(jit, proto, pc);
    if (existing) return true;
    
    return ljit_hotcount_call(&jit->hotcounts, pc);
}

/**
 * @brief 处理侧边出口
 * @param jit JIT引擎
 * @param trace Trace
 * @param exit_id 出口ID
 * @return 处理结果
 */
JitError ljit_handle_exit(JitEngine *jit, Trace *trace, uint32_t exit_id) {
    jit->stats.exit_count++;
    
    /* 检查是否需要编译侧边Trace */
    if (ljit_hotcount_exit(&jit->hotcounts, exit_id)) {
        /* TODO: 编译侧边Trace */
        (void)trace;
    }
    
    return JIT_OK;
}

/* ========================================================================
 * 编译流程
 * ======================================================================== */

/**
 * @brief 开始Trace记录
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 起始PC
 * @return 成功返回JIT_OK
 */
JitError ljit_trace_start(JitEngine *jit, Proto *proto, uint32_t pc) {
    if (jit->ctx.state != JIT_STATE_IDLE) {
        return JIT_ERR_BLACKLIST;
    }
    
    jit->stats.trace_start++;
    jit->ctx.state = JIT_STATE_RECORDING;
    
    return ljit_rec_start(&jit->recorder, proto, pc);
}

/**
 * @brief 记录字节码
 * @param jit JIT引擎
 * @param ins 字节码指令
 * @return 继续记录返回true
 */
bool ljit_trace_record(JitEngine *jit, uint64_t ins) {
    if (jit->ctx.state != JIT_STATE_RECORDING) {
        return false;
    }
    
    return ljit_rec_ins(&jit->recorder, ins);
}

/**
 * @brief 结束并编译Trace
 * @param jit JIT引擎
 * @return 编译后的Trace，失败返回NULL
 */
Trace *ljit_trace_finish(JitEngine *jit) {
    Trace *trace = ljit_rec_end(&jit->recorder);
    
    if (!trace) {
        jit->ctx.state = JIT_STATE_IDLE;
        jit->stats.trace_abort++;
        return NULL;
    }
    
    /* 编译Trace */
    JitError err = ljit_compile(jit, trace);
    if (err != JIT_OK) {
        jit->ctx.state = JIT_STATE_IDLE;
        jit->stats.trace_abort++;
        return NULL;
    }
    
    jit->ctx.state = JIT_STATE_IDLE;
    jit->stats.trace_success++;
    
    return trace;
}

/**
 * @brief 中止Trace记录
 * @param jit JIT引擎
 * @param reason 中止原因
 */
void ljit_trace_abort(JitEngine *jit, AbortReason reason) {
    ljit_rec_abort(&jit->recorder, reason);
    jit->ctx.state = JIT_STATE_IDLE;
    jit->stats.trace_abort++;
}

/**
 * @brief 编译Trace
 * @param jit JIT引擎
 * @param trace Trace结构
 * @return 成功返回JIT_OK
 */
JitError ljit_compile(JitEngine *jit, Trace *trace) {
    jit->ctx.state = JIT_STATE_COMPILING;
    
    /* 优化 */
    if (jit->config.enable_opt && jit->recorder.builder) {
        ljit_opt_init(&jit->opt, jit->recorder.builder);
        jit->opt.config = jit->config.opt;
        ljit_opt_run(&jit->opt);
        ljit_opt_free(&jit->opt);
    }
    
    /* 分配机器码内存 */
    void *code_mem;
    size_t code_size;
    JitError err = ljit_mem_reserve(&jit->mem, 4096, &code_mem, &code_size);
    if (err != JIT_OK) {
        return err;
    }
    
    /* 初始化发射器 */
    err = ljit_emit_init(&jit->emitter, jit->recorder.builder, code_mem, code_size);
    if (err != JIT_OK) {
        return err;
    }
    
    /* 生成代码 */
    err = ljit_emit_trace(&jit->emitter, trace);
    if (err != JIT_OK) {
        ljit_emit_free(&jit->emitter);
        return err;
    }
    
    /* 提交内存 */
    ljit_mem_commit(&jit->mem, trace->mcode_size);
    
    /* 设置为可执行 */
    ljit_mem_protect_exec(&jit->mem, trace->mcode, trace->mcode_size);
    ljit_mem_flush_icache(trace->mcode, trace->mcode_size);
    
    jit->stats.mcode_size += trace->mcode_size;
    
    ljit_emit_free(&jit->emitter);
    
    return JIT_OK;
}

/* ========================================================================
 * Trace执行
 * ======================================================================== */

/**
 * @brief 查找已编译的Trace
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 字节码PC
 * @return 找到的Trace，未找到返回NULL
 */
Trace *ljit_find_trace(JitEngine *jit, Proto *proto, uint32_t pc) {
    for (uint32_t i = 0; i < jit->ctx.trace_count; i++) {
        Trace *t = jit->ctx.traces[i];
        if (t && t->proto == proto && t->start_pc == pc) {
            return t;
        }
    }
    return NULL;
}

/**
 * @brief 执行编译后的Trace
 * @param jit JIT引擎
 * @param trace Trace
 * @return 执行结果
 */
JitError ljit_execute(JitEngine *jit, Trace *trace) {
    if (!trace || !trace->mcode) {
        return JIT_ERR_BLACKLIST;
    }
    
    jit->ctx.state = JIT_STATE_RUNNING;
    jit->stats.trace_exec++;
    
    /* 调用生成的机器码 */
    typedef int (*JitFunc)(void *L);
    JitFunc func = (JitFunc)trace->mcode;
    int result = func(jit->L);
    
    jit->ctx.state = JIT_STATE_IDLE;
    
    (void)result;
    return JIT_OK;
}

/**
 * @brief 获取Trace入口点
 * @param trace Trace
 * @return 函数指针
 */
void *ljit_trace_entry(Trace *trace) {
    return trace ? trace->mcode : NULL;
}

/* ========================================================================
 * VM集成钩子
 * ======================================================================== */

/**
 * @brief VM循环后向跳转钩子
 * @param jit JIT引擎
 * @param proto 函数原型
 * @param pc 目标PC
 * @return 如果应该执行JIT代码返回true
 */
bool ljit_vm_loop(JitEngine *jit, Proto *proto, uint32_t pc) {
    if (!ljit_is_enabled(jit)) return false;
    
    /* 检查是否有已编译的Trace */
    Trace *trace = ljit_find_trace(jit, proto, pc);
    if (trace && trace->mcode) {
        ljit_execute(jit, trace);
        return true;
    }
    
    /* 检查热点 */
    if (ljit_check_hotloop(jit, proto, pc)) {
        if (ljit_trace_start(jit, proto, pc) == JIT_OK) {
            /* 开始记录 */
            return false;
        }
    }
    
    return false;
}

/**
 * @brief VM调用钩子
 * @param jit JIT引擎
 * @param proto 被调用函数
 * @return 如果应该执行JIT代码返回true
 */
bool ljit_vm_call(JitEngine *jit, Proto *proto) {
    if (!ljit_is_enabled(jit)) return false;
    
    Trace *trace = ljit_find_trace(jit, proto, 0);
    if (trace && trace->mcode) {
        ljit_execute(jit, trace);
        return true;
    }
    
    return false;
}

/**
 * @brief VM返回钩子
 * @param jit JIT引擎
 */
void ljit_vm_return(JitEngine *jit) {
    /* 如果正在记录，处理返回 */
    if (jit->ctx.state == JIT_STATE_RECORDING) {
        if (jit->recorder.depth == 0) {
            /* 根级返回，结束记录 */
            ljit_trace_finish(jit);
        }
    }
}

/* ========================================================================
 * 统计和调试
 * ======================================================================== */

static const char *error_messages[] = {
    "success",
    "memory allocation failed",
    "not yet implemented",
    "function blacklisted",
    "trace length exceeded",
    "loop depth exceeded",
    "type unstable",
    "too many side exits"
};

/**
 * @brief 获取错误消息
 * @param error 错误码
 * @return 错误消息字符串
 */
const char *ljit_error_str(JitError error) {
    if (error < sizeof(error_messages) / sizeof(error_messages[0])) {
        return error_messages[error];
    }
    return "unknown error";
}

/**
 * @brief 打印JIT统计信息
 * @param jit JIT引擎
 */
void ljit_dump_stats(JitEngine *jit) {
    printf("=== JIT Statistics ===\n");
    printf("Enabled: %s\n", jit->config.enable ? "yes" : "no");
    printf("State: %d\n", jit->ctx.state);
    printf("\nTrace stats:\n");
    printf("  Start attempts: %llu\n", (unsigned long long)jit->stats.trace_start);
    printf("  Successful: %llu\n", (unsigned long long)jit->stats.trace_success);
    printf("  Aborted: %llu\n", (unsigned long long)jit->stats.trace_abort);
    printf("  Executions: %llu\n", (unsigned long long)jit->stats.trace_exec);
    printf("  Side exits: %llu\n", (unsigned long long)jit->stats.exit_count);
    printf("  Total traces: %u\n", jit->ctx.trace_count);
    printf("\nMemory stats:\n");
    printf("  Machine code: %llu bytes\n", (unsigned long long)jit->stats.mcode_size);
    
    size_t total, used, avail;
    ljit_mem_stats(&jit->mem, &total, &used, &avail);
    printf("  Code memory: %zu / %zu bytes (%.1f%%)\n",
           used, total, total > 0 ? 100.0 * used / total : 0);
}

/**
 * @brief 打印所有Trace
 * @param jit JIT引擎
 */
void ljit_dump_traces(JitEngine *jit) {
    printf("=== Traces (%u total) ===\n", jit->ctx.trace_count);
    
    for (uint32_t i = 0; i < jit->ctx.trace_count; i++) {
        Trace *t = jit->ctx.traces[i];
        if (!t) continue;
        
        printf("[%u] proto=%p pc=%u type=%d ir_count=%u mcode=%zu bytes\n",
               t->id, (void *)t->proto, t->start_pc, t->type,
               t->ir_count, t->mcode_size);
    }
}

/**
 * @brief 打印单个Trace
 * @param jit JIT引擎
 * @param trace_id Trace ID
 */
void ljit_dump_trace(JitEngine *jit, uint32_t trace_id) {
    for (uint32_t i = 0; i < jit->ctx.trace_count; i++) {
        Trace *t = jit->ctx.traces[i];
        if (t && t->id == trace_id) {
            printf("=== Trace %u ===\n", trace_id);
            printf("Proto: %p\n", (void *)t->proto);
            printf("Start PC: %u\n", t->start_pc);
            printf("Type: %d\n", t->type);
            printf("IR count: %u\n", t->ir_count);
            printf("Const count: %u\n", t->const_count);
            printf("Machine code: %zu bytes at %p\n", t->mcode_size, t->mcode);
            printf("Exits: %u\n", t->exit_count);
            return;
        }
    }
    
    printf("Trace %u not found\n", trace_id);
}
