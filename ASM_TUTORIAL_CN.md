# XCLua 内联汇编教程

XCLua 提供了一个强大的内联汇编功能，允许您直接将原始 Lua 虚拟机指令注入到 Lua 代码中。这对于性能优化、底层操作或实现标准 Lua 无法表达的功能非常有用。

## 基本语法

内联汇编写在 `asm(...)` 块内。指令用分号（`;`）分隔。

```lua
asm(
    LOADI R0 100;
    RETURN1 R0;
)
```

## 操作数与修饰符

汇编器支持多种修饰符，以便更轻松地处理 Lua 的内部结构。

### 寄存器
*   **原始寄存器**: `R0`, `R1`, `R2`, ... (直接访问寄存器)
*   **局部变量**: `$varname` (解析为分配给局部变量 `varname` 的寄存器)
*   **分配的寄存器**: 参见下面的 `newreg`。

### 常量
*   **整数**: `#123` (用于立即数操作数，如 `LOADI`)
*   **浮点数**: `#3.14` (用于 `LOADF`)
*   **字符串**: `#"string"` (将字符串添加到常量表并返回索引)
*   **添加到常量池**:
    *   `#K 123`: 将整数 123 添加到常量池并返回索引。
    *   `#KF 3.14`: 将浮点数 3.14 添加到常量池并返回索引。

### 上值 (Upvalues)
*   **上值索引**: `^varname` (解析为 `varname` 的上值索引)

### 标签与跳转
*   **定义标签**: `:label_name`
*   **引用标签**: `@label_name` (返回标签的 PC 位置)
*   **当前 PC**: `@`

### 特殊值
*   `!freereg`: 当前栈顶 (第一个空闲寄存器)。
*   `!pc`: 当前指令指针。
*   `!nk`: 常量数量。

## 伪指令与辅助指令

### `newreg name`
在栈上分配一个新寄存器，并将 `name` 定义为其别名。这确保编译器知道该寄存器正在使用中。

```lua
asm(
    newreg temp;
    LOADI temp 42;
)
```

### `getglobal reg "name"`
将全局变量加载到寄存器中。

```lua
asm(
    newreg r;
    getglobal r "print";
)
```

### `setglobal reg "name"`
从寄存器设置全局变量。

```lua
asm(
    newreg val;
    LOADI val 100;
    setglobal val "my_global";
)
```

### `def name value`
定义局部汇编常量。

```lua
asm(
    def MY_CONST 10;
    LOADI R0 MY_CONST;
)
```

### 条件汇编 (`_if` / `_else` / `_endif`)
允许根据静态值有条件地编译指令。支持比较运算符。

```lua
asm(
    _if 1 == 1
        _print "Condition is true"
    _else
        _print "Condition is false"
    _endif
)
```

### `jmpx @label` / `JMP @label`
跳转到标签。`jmpx` 是一个显式计算相对偏移量的辅助工具，但标准的 `JMP` 也能正确支持 `@label`。

```lua
asm(
    :loop;
    ...
    JMP @loop;
)
```

### `junk`
插入垃圾数据以对抗反汇编。可以是一个字符串，它将被编码为 `EXTRAARG` 指令序列。也可以是一个整数，它将生成指定数量的 `NOP` 指令。

```lua
asm(
    junk "some_random_string_data"
    junk 5
)
```

### 调试
*   `_print "msg" val`: 在编译期间打印消息和可选值。
*   `_assert cond`: 在编译时断言条件。

## 示例

### 1. 简单的数学运算
```lua
local result
asm(
    newreg a;
    newreg b;
    LOADI a 10;
    LOADI b 20;
    ADD a a b;
    MOVE $result a;
)
print(result) -- 30
```

### 2. 全局函数调用
```lua
asm(
    newreg func;
    newreg arg;
    getglobal func "print";
    LOADK arg #"Hello from ASM!";
    CALL func 2 1; -- 2 args (func + arg), 1 result (void)
)
```

### 3. 自定义循环
```lua
local sum = 0
asm(
    newreg cnt;
    newreg acc;
    LOADI cnt 5;
    LOADI acc 0;

    :loop;
    ADD acc acc cnt;
    SUBK cnt cnt #K 1; -- 减去常量 1

    -- 检查 cnt > 0
    EQI cnt 0 0; -- 如果 cnt == 0 则跳过下一条指令
    JMP @loop;

    MOVE $sum acc;
)
print(sum) -- 15
```
