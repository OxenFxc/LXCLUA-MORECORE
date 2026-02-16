# LuaJIT 迁移指南 (LUAJIT_MIGRATION_GUIDE.md)

本指南旨在说明如何将当前的 `LXCLUA-NCore` 项目（基于 Lua 5.5 + asmjit 自定义 JIT）迁移到 LuaJIT（基于 Lua 5.1 + Trace JIT）。

## ⚠️ 重要警告

**这不是简单的“替换 JIT 后端”。**

LuaJIT 是一个独立的 Lua 虚拟机实现，其内部架构与标准 Lua（PUC-Lua）完全不同：
1.  **版本差异**：LuaJIT 基于 **Lua 5.1** 语法（部分支持 5.2），而当前项目基于 **Lua 5.5**。迁移意味着**降级**语言版本。
2.  **内部结构**：
    *   LuaJIT 使用 "NaN Tagging" (64位浮点数标记) 来表示值 (`TValue`)。
    *   当前项目使用标准 Lua 的 `TValue` 结构（标签+值）。
    *   LuaJIT 的垃圾回收器 (GC) 和表实现完全不同。
3.  **扩展不兼容**：当前项目中的 C 扩展（如 `SuperStruct`）深度依赖 Lua 5.5 的内部 API（如 `ttisinteger`, `luaM_newvector` 等），这些在 LuaJIT 中不存在或用法完全不同。

---

## 迁移步骤

如果你确定要迁移到 LuaJIT 以获取极致性能（且愿意牺牲 Lua 5.5 的新特性），请按照以下步骤操作：

### 1. 替换核心引擎 (Core Replacement)

当前的 `jit_backend.cpp` 和 `lvm.c` 等文件是基于 Lua 5.5 的。你需要完全移除当前的 Lua 核心代码。

*   **操作**：删除除了 `main.c` 和扩展模块（`lsuper.c`, `lclass.c` 等）之外的所有 Lua 源码文件。
*   **下载**：获取 [LuaJIT 2.1](https://github.com/LuaJIT/LuaJIT) 的源码。
*   **集成**：将 LuaJIT 的源码（`src` 目录下的 `.c` 和 `.h` 文件，以及构建所需的 `minilua` 等工具）放入项目目录。

### 2. 重写 C 扩展 (Rewrite Extensions)

这是最困难的一步。`SuperStruct` (`lsuper.c`) 和其他扩展使用了 Lua 5.5 的内部宏和 API。

*   **问题**：LuaJIT 没有 `LUA_VNUMINT`（整数类型标签），没有 `lua_newuserdatauv`（用户数据关联值），也没有相同的 `TValue` 访问宏。
*   **解决方案 A（标准 C API）**：
    *   将所有直接访问 `TValue`、`StkId` 或内部结构的代码，重写为使用标准的 **Lua 5.1 C API**（如 `lua_getfield`, `lua_settable`, `lua_newuserdata`）。
    *   这会损失一些性能，但兼容性最好。
*   **解决方案 B（LuaJIT FFI）**：
    *   利用 LuaJIT 的 `FFI` 库，在 Lua 层面重新实现 `SuperStruct` 的逻辑，直接调用 C 函数进行内存操作。这是 LuaJIT 推荐的高性能扩展方式。

### 3. 更新构建系统 (Update Build System)

LuaJIT 的构建过程比标准 Lua 复杂，因为它需要先编译 `minilua` 来生成平台相关的头文件和汇编代码（DynASM）。

*   **修改 Makefile/Android.mk**：
    *   你需要引入 LuaJIT 的构建规则（参考 LuaJIT 的 `src/Makefile`）。
    *   移除对 `asmjit` 的依赖（LuaJIT 自带汇编器 DynASM）。
    *   确保链接时包含 `libluajit.a` 而不是之前的 `liblua.a`。

### 4. 降级 Lua 脚本 (Script Downgrade)

由于 LuaJIT 主要兼容 Lua 5.1，你需要修改现有的 `.lua` 脚本：

*   **移除/替换**：
    *   `_ENV` 环境控制（Lua 5.2+ 特性）。
    *   位运算符 (`&`, `|`, `~`, `<<`, `>>`)：LuaJIT 使用 `bit` 库（如 `bit.band`, `bit.bor`）。
    *   整数除法 (`//`)：LuaJIT 不支持。
    *   `goto` 语句（LuaJIT 支持，但限制可能不同）。
    *   `__pairs` / `__ipairs` 元方法（LuaJIT 5.2 兼容模式下支持，但需开启）。

---

## 替代方案：优化现有 JIT

如果迁移成本过高，建议继续优化当前的 `asmjit` 实现：

1.  **类型推断与特化 (Type Specialization)**：在 `jit_backend.cpp` 中增加对常见类型（如纯数字计算）的特化路径，减少类型检查。
2.  **寄存器分配 (Register Allocation)**：目前 JIT 可能频繁读写栈内存。引入简单的线性扫描寄存器分配算法，将热点变量保留在寄存器中。
3.  **内联缓存 (Inline Caching)**：优化 `OP_GETTABLE` / `OP_SETTABLE`，缓存上一轮查找的结果，避免每次都调用 `luaH_get`。

---

**总结**：切换到 LuaJIT 等同于更换整个游戏引擎。除非你确实需要 LuaJIT 的 Trace JIT 特性且能接受 Lua 5.1 的限制，否则优化现有的 Lua 5.5 JIT 是更稳健的选择。
