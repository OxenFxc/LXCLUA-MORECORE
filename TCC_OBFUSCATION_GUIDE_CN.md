# TCC 混淆编译指南

本文档介绍如何使用 `tcc` 库将 Lua 代码编译为经过混淆的 C 代码，并最终生成可加载的共享库（.so/.dll）。

## 1. 基本概述

`tcc` 模块提供了将 Lua 源代码编译为 C 代码的功能。在编译过程中，可以开启多种混淆选项，如控制流扁平化（Control Flow Flattening）、虚拟化保护（VM Protect）和字符串加密等，以提高代码的安全性。

## 2. API 说明

### `tcc.compile(code, [modname], options)`

将 Lua 代码编译为 C 代码字符串。

- **参数**：
    - `code`: (string) 需要编译的 Lua 源代码。
    - `modname`: (string, 可选) 生成的模块名称，默认为 "module"。生成的 C 代码中入口函数将命名为 `luaopen_<modname>`。建议与最终生成的动态库文件名一致（不含扩展名）。
    - `options`: (table) 编译选项表。

- **`options` 表结构**：
    - `obfuscate`: (boolean) 是否开启 API 混淆（隐藏 Lua API 调用）。
    - `flags`: (integer) 混淆标志位掩码，用于控制具体开启哪些混淆算法。
    - `seed`: (integer) 混淆随机种子，用于保证生成结果的确定性。
    - `use_pure_c`: (boolean) 是否使用纯 C 标准库（如 math.h）替代 Lua API 进行算术运算。
    - `flatten`: (boolean, 简便写法) 等同于开启 `OBFUSCATE_CFF` 标志。
    - `string_encryption`: (boolean, 简便写法) 等同于开启 `OBFUSCATE_STR_ENCRYPT` 标志。

### `tcc.compute_flags(flag_table)`

辅助函数，用于将易读的选项表转换为 `flags` 整数掩码。

- **参数**：
    - `flag_table`: (table)包含布尔值的选项表。

- **支持的选项键**：
    - `flatten`: 控制流扁平化 (Control Flow Flattening)。
    - `block_shuffle`: 基本块乱序。
    - `bogus_blocks`: 插入虚假基本块（增加分析难度，但可能影响性能或稳定性）。
    - `state_encode`: 状态变量编码。
    - `nested_dispatcher`: 嵌套分发器（两级状态机）。
    - `opaque_predicates`: 不透明谓词（插入恒真/恒假条件）。**注意：在某些情况下可能引发类型错误，请谨慎使用。**
    - `func_interleave`: 函数交织（将多个函数体混合在一起）。
    - `vm_protect`: 虚拟化保护（使用自定义字节码指令集）。
    - `binary_dispatcher`: 二分查找分发器。
    - `random_nop`: 插入随机 NOP 指令。
    - `string_encryption`: 字符串加密。

## 3. 完整示例

以下脚本演示了如何将一段 Lua 代码编译为混淆后的 C 代码。

```lua
-- build_script.lua
local tcc = require("tcc")

-- 1. 待编译的 Lua 源代码
local code = [[
return function(a, b)
    print("正在计算 " .. a .. " + " .. b)
    local sum = 0
    for i = 1, a do
        sum = sum + b
    end
    return sum
end
]]

-- 2. 计算混淆标志
-- 推荐配置：扁平化 + 字符串加密 + 乱序
local flags = tcc.compute_flags({
    flatten = true,
    string_encryption = true,
    block_shuffle = true,
    -- opaque_predicates = true -- 可选，如果遇到运行时错误请关闭此项
})

print("混淆标志值: " .. flags)

-- 3. 编译选项
local modname = "my_secret_lib" -- 模块名，对应文件名 my_secret_lib.so
local options = {
    obfuscate = true, -- 开启 API 隐藏
    flags = flags,    -- 应用混淆算法
    seed = 12345      -- 固定种子以便复现
}

-- 4. 执行编译
local c_code = tcc.compile(code, modname, options)

-- 5. 保存为 C 文件
local f = io.open("my_secret_lib.c", "w")
f:write(c_code)
f:close()

print("已生成 C 代码: my_secret_lib.c")
```

## 4. 编译与使用

### 步骤 1：生成 C 代码
运行上述构建脚本：
```bash
./lxclua build_script.lua
```

### 步骤 2：编译为动态库
使用 GCC 将生成的 C 代码编译为共享库（.so 或 .dll）。
需要确保包含 Lua 头文件路径（通常在当前目录或系统包含目录）。

**Linux/macOS:**
```bash
gcc -O2 -shared -fPIC -o my_secret_lib.so my_secret_lib.c -I.
```

**Windows (MinGW):**
```bash
gcc -O2 -shared -o my_secret_lib.dll my_secret_lib.c -I. -llua54
```

### 步骤 3：在 Lua 中加载使用
编译完成后，可以直接在 Lua 中 `require` 该模块。

```lua
-- main.lua
-- 确保 .so 文件在 LUA_CPATH 路径中
local secret_func = require("my_secret_lib")

local result = secret_func(10, 5)
print("计算结果: " .. result)
```

## 5. 注意事项

1. **模块名称匹配**：`tcc.compile` 的 `modname` 参数必须与最终生成的动态库文件名一致（不含扩展名）。例如 `modname="abc"` 对应 `abc.so`，`require "abc"` 会查找 `luaopen_abc`。
2. **不透明谓词风险**：`opaque_predicates` 选项会生成随机的运算指令。如果这些指令操作了错误的类型（例如对字符串进行位运算），可能会导致运行时错误。如果遇到此类问题，请禁用该选项。
3. **VM 保护**：`vm_protect` 提供了极强的保护能力，但会显著增加代码体积和降低执行效率。请根据实际需求权衡使用。
