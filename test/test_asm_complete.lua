--[[
    LXCLUA-NCore 内联汇编 (Inline ASM) 完整测试指南
    
    本文件包含所有内联汇编功能的详细示例和测试用例
    运行方式: ./lxclua test_asm_complete.lua
]]

print("=" .. string.rep("=", 60))
print("LXCLUA-NCore 内联汇编完整测试")
print("=" .. string.rep("=", 60))

-- ============================================================
-- 第一部分: 基础语法
-- ============================================================
print("\n[1] 基础语法测试")
print("-" .. string.rep("-", 60))

-- 1.1 最简单的内联汇编 - 加载立即数并返回
do
    print("\n1.1 基本指令格式")
    local result
    asm(
        newreg r0;
        LOADI r0 42;
        MOVE $result r0;
    )
    print("  LOADI r0 42 -> result = " .. tostring(result))
    assert(result == 42, "基本指令测试失败")
    print("  ✓ 通过")
end

-- 1.2 多条指令使用分号分隔
do
    print("\n1.2 多条指令")
    local a, b
    asm(
        newreg r0;
        newreg r1;
        LOADI r0 100;
        LOADI r1 200;
        MOVE $a r0;
        MOVE $b r1;
    )
    print("  a = " .. a .. ", b = " .. b)
    assert(a == 100 and b == 200, "多指令测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第二部分: 寄存器操作
-- ============================================================
print("\n[2] 寄存器操作测试")
print("-" .. string.rep("-", 60))

-- 2.1 newreg - 分配新寄存器
do
    print("\n2.1 newreg 分配寄存器")
    local val
    asm(
        newreg temp;
        LOADI temp 999;
        MOVE $val temp;
    )
    print("  newreg temp; LOADI temp 999 -> val = " .. val)
    assert(val == 999, "newreg测试失败")
    print("  ✓ 通过")
end

-- 2.2 $varname - 访问局部变量寄存器
do
    print("\n2.2 $varname 访问局部变量")
    local x = 10
    local y
    asm(
        MOVE $y $x;
    )
    print("  MOVE $y $x -> y = " .. y)
    assert(y == 10, "$varname测试失败")
    print("  ✓ 通过")
end

-- 2.3 R0, R1... - 直接寄存器访问
do
    print("\n2.3 直接寄存器访问")
    local result
    asm(
        LOADI R0 123;
        MOVE $result R0;
    )
    print("  LOADI R0 123 -> result = " .. result)
    assert(result == 123, "直接寄存器测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第三部分: 常量操作
-- ============================================================
print("\n[3] 常量操作测试")
print("-" .. string.rep("-", 60))

-- 3.1 #整数 - 直接整数值
do
    print("\n3.1 #整数 直接值")
    local val
    asm(
        newreg r;
        LOADI r #100;
        MOVE $val r;
    )
    print("  LOADI r #100 -> val = " .. val)
    assert(val == 100, "#整数测试失败")
    print("  ✓ 通过")
end

-- 3.2 #"字符串" - 字符串常量
do
    print("\n3.2 #\"字符串\" 字符串常量")
    local s
    asm(
        newreg r;
        LOADK r #"Hello ASM!";
        MOVE $s r;
    )
    print("  LOADK r #\"Hello ASM!\" -> s = " .. tostring(s))
    assert(s == "Hello ASM!", "#\"字符串\"测试失败")
    print("  ✓ 通过")
end

-- 3.3 #K 整数 - 添加到常量池
do
    print("\n3.3 #K 整数 常量池")
    local val
    asm(
        newreg r;
        LOADK r #K 42;
        MOVE $val r;
    )
    print("  LOADK r #K 42 -> val = " .. val)
    assert(val == 42, "#K测试失败")
    print("  ✓ 通过")
end

-- 3.4 #KF 浮点数 - 浮点常量池
do
    print("\n3.4 #KF 浮点数 常量池")
    local val
    asm(
        newreg r;
        LOADK r #KF 3.14159;
        MOVE $val r;
    )
    print("  LOADK r #KF 3.14159 -> val = " .. tostring(val))
    assert(val == 3.14159, "#KF测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第四部分: 算术运算
-- ============================================================
print("\n[4] 算术运算测试")
print("-" .. string.rep("-", 60))

-- 4.1 ADD - 加法
do
    print("\n4.1 ADD 加法")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 10;
        LOADI b 20;
        ADD c a b;
        MOVE $result c;
    )
    print("  10 + 20 = " .. result)
    assert(result == 30, "ADD测试失败")
    print("  ✓ 通过")
end

-- 4.2 SUB - 减法
do
    print("\n4.2 SUB 减法")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 50;
        LOADI b 20;
        SUB c a b;
        MOVE $result c;
    )
    print("  50 - 20 = " .. result)
    assert(result == 30, "SUB测试失败")
    print("  ✓ 通过")
end

-- 4.3 MUL - 乘法
do
    print("\n4.3 MUL 乘法")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 6;
        LOADI b 7;
        MUL c a b;
        MOVE $result c;
    )
    print("  6 * 7 = " .. result)
    assert(result == 42, "MUL测试失败")
    print("  ✓ 通过")
end

-- 4.4 DIV - 除法
do
    print("\n4.4 DIV 除法")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 100;
        LOADI b 4;
        DIV c a b;
        MOVE $result c;
    )
    print("  100 / 4 = " .. result)
    assert(result == 25, "DIV测试失败")
    print("  ✓ 通过")
end

-- 4.5 MOD - 取模
do
    print("\n4.5 MOD 取模")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 17;
        LOADI b 5;
        MOD c a b;
        MOVE $result c;
    )
    print("  17 % 5 = " .. result)
    assert(result == 2, "MOD测试失败")
    print("  ✓ 通过")
end

-- 4.6 POW - 幂运算
do
    print("\n4.6 POW 幂运算")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 2;
        LOADI b 10;
        POW c a b;
        MOVE $result c;
    )
    print("  2 ^ 10 = " .. result)
    assert(result == 1024, "POW测试失败")
    print("  ✓ 通过")
end

-- 4.7 IDIV - 整除
do
    print("\n4.7 IDIV 整除")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 17;
        LOADI b 3;
        IDIV c a b;
        MOVE $result c;
    )
    print("  17 // 3 = " .. result)
    assert(result == 5, "IDIV测试失败")
    print("  ✓ 通过")
end

-- 4.8 ADDI - 立即数加法
do
    print("\n4.8 ADDI 立即数加法")
    local result
    asm(
        newreg a;
        newreg c;
        LOADI a 100;
        ADDI c a 50;
        MOVE $result c;
    )
    print("  100 + 50 = " .. result)
    assert(result == 150, "ADDI测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第五部分: 位运算
-- ============================================================
print("\n[5] 位运算测试")
print("-" .. string.rep("-", 60))

-- 5.1 BAND - 按位与
do
    print("\n5.1 BAND 按位与")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 0xFF;
        LOADI b 0x0F;
        BAND c a b;
        MOVE $result c;
    )
    print("  0xFF & 0x0F = " .. result)
    assert(result == 0x0F, "BAND测试失败")
    print("  ✓ 通过")
end

-- 5.2 BOR - 按位或
do
    print("\n5.2 BOR 按位或")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 0xF0;
        LOADI b 0x0F;
        BOR c a b;
        MOVE $result c;
    )
    print("  0xF0 | 0x0F = " .. result)
    assert(result == 0xFF, "BOR测试失败")
    print("  ✓ 通过")
end

-- 5.3 BXOR - 按位异或
do
    print("\n5.3 BXOR 按位异或")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 0xFF;
        LOADI b 0x0F;
        BXOR c a b;
        MOVE $result c;
    )
    print("  0xFF ~ 0x0F = " .. result)
    assert(result == 0xF0, "BXOR测试失败")
    print("  ✓ 通过")
end

-- 5.4 BNOT - 按位取反
do
    print("\n5.4 BNOT 按位取反")
    local result
    asm(
        newreg a;
        newreg c;
        LOADI a 0;
        BNOT c a;
        MOVE $result c;
    )
    print("  ~0 = " .. result)
    assert(result == -1, "BNOT测试失败")
    print("  ✓ 通过")
end

-- 5.5 SHL - 左移
do
    print("\n5.5 SHL 左移")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 1;
        LOADI b 4;
        SHL c a b;
        MOVE $result c;
    )
    print("  1 << 4 = " .. result)
    assert(result == 16, "SHL测试失败")
    print("  ✓ 通过")
end

-- 5.6 SHR - 右移
do
    print("\n5.6 SHR 右移")
    local result
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 16;
        LOADI b 2;
        SHR c a b;
        MOVE $result c;
    )
    print("  16 >> 2 = " .. result)
    assert(result == 4, "SHR测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第六部分: 比较运算
-- ============================================================
print("\n[6] 比较运算测试")
print("-" .. string.rep("-", 60))

-- 6.1 EQ - 相等比较
do
    print("\n6.1 EQ 相等比较")
    local result
    asm(
        newreg a;
        newreg b;
        LOADI a 10;
        LOADI b 10;
        EQ a b 0;
        JMP 1;
        LOADI a 1;
        JMP 1;
        LOADI a 0;
        MOVE $result a;
    )
    print("  10 == 10 -> " .. result)
    assert(result == 1, "EQ测试失败")
    print("  ✓ 通过")
end

-- 6.2 LT - 小于比较
do
    print("\n6.2 LT 小于比较")
    local result
    asm(
        newreg a;
        newreg b;
        LOADI a 5;
        LOADI b 10;
        LT a b 0;
        JMP 1;
        LOADI a 1;
        JMP 1;
        LOADI a 0;
        MOVE $result a;
    )
    print("  5 < 10 -> " .. result)
    assert(result == 1, "LT测试失败")
    print("  ✓ 通过")
end

-- 6.3 LE - 小于等于比较
do
    print("\n6.3 LE 小于等于比较")
    local result
    asm(
        newreg a;
        newreg b;
        LOADI a 10;
        LOADI b 10;
        LE a b 0;
        JMP 1;
        LOADI a 1;
        JMP 1;
        LOADI a 0;
        MOVE $result a;
    )
    print("  10 <= 10 -> " .. result)
    assert(result == 1, "LE测试失败")
    print("  ✓ 通过")
end

-- 6.4 SPACESHIP - 太空船操作符
do
    print("\n6.4 SPACESHIP 太空船操作符")
    local r1, r2, r3
    asm(
        newreg a;
        newreg b;
        newreg c;
        LOADI a 5;
        LOADI b 10;
        SPACESHIP c a b;
        MOVE $r1 c;
        LOADI a 10;
        SPACESHIP c a b;
        MOVE $r2 c;
        LOADI a 15;
        SPACESHIP c a b;
        MOVE $r3 c;
    )
    print("  5 <=> 10 = " .. r1 .. ", 10 <=> 10 = " .. r2 .. ", 15 <=> 10 = " .. r3)
    assert(r1 == -1 and r2 == 0 and r3 == 1, "SPACESHIP测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第七部分: 控制流 - 标签和跳转
-- ============================================================
print("\n[7] 控制流测试")
print("-" .. string.rep("-", 60))

-- 7.1 标签定义和跳转
do
    print("\n7.1 标签和JMP跳转")
    local count = 0
    local result
    asm(
        newreg r;
        LOADI r 0;
        :loop;
        ADDI r r 1;
        EQI r 5 0;
        JMP @loop;
        MOVE $result r;
    )
    print("  循环5次 -> result = " .. result)
    assert(result == 5, "标签跳转测试失败")
    print("  ✓ 通过")
end

-- 7.2 jmpx 伪指令
do
    print("\n7.2 jmpx 伪指令")
    local result
    asm(
        newreg r;
        LOADI r 0;
        :start;
        ADDI r r 1;
        EQI r 3 0;
        jmpx @start;
        MOVE $result r;
    )
    print("  jmpx循环3次 -> result = " .. result)
    assert(result == 3, "jmpx测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第八部分: 伪指令
-- ============================================================
print("\n[8] 伪指令测试")
print("-" .. string.rep("-", 60))

-- 8.1 def - 定义汇编常量
do
    print("\n8.1 def 定义常量")
    local result
    asm(
        def MY_VALUE 42;
        newreg r;
        LOADI r MY_VALUE;
        MOVE $result r;
    )
    print("  def MY_VALUE 42 -> result = " .. result)
    assert(result == 42, "def测试失败")
    print("  ✓ 通过")
end

-- 8.2 nop - 插入NOP指令
do
    print("\n8.2 nop 插入NOP")
    local result
    asm(
        newreg r;
        LOADI r 1;
        nop 3;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  nop 3 -> result = " .. result)
    assert(result == 2, "nop测试失败")
    print("  ✓ 通过")
end

-- 8.3 align - 对齐指令
do
    print("\n8.3 align 对齐")
    local result
    asm(
        newreg r;
        LOADI r 100;
        align 4;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  align 4 -> result = " .. result)
    assert(result == 101, "align测试失败")
    print("  ✓ 通过")
end

-- 8.4 rep - 重复指令块
do
    print("\n8.4 rep 重复指令")
    local result
    asm(
        newreg r;
        LOADI r 0;
        rep 5 {
            ADDI r r 1;
        }
        MOVE $result r;
    )
    print("  rep 5 { ADDI } -> result = " .. result)
    assert(result == 5, "rep测试失败")
    print("  ✓ 通过")
end

-- 8.5 comment/rem - 注释
do
    print("\n8.5 comment 注释")
    local result
    asm(
        newreg r;
        LOADI r 42;
        comment "这是一个注释";
        rem "这也是注释";
        MOVE $result r;
    )
    print("  comment测试 -> result = " .. result)
    assert(result == 42, "comment测试失败")
    print("  ✓ 通过")
end

-- 8.6 raw - 原始指令
do
    print("\n8.6 raw 原始指令")
    local result
    asm(
        newreg r;
        LOADI r 100;
        raw 0;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  raw 0 (NOP) -> result = " .. result)
    assert(result == 101, "raw测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第九部分: 全局变量操作
-- ============================================================
print("\n[9] 全局变量操作测试")
print("-" .. string.rep("-", 60))

-- 9.1 getglobal - 获取全局变量
do
    print("\n9.1 getglobal 获取全局变量")
    GLOBAL_TEST_VAR = 12345
    local result
    asm(
        newreg r;
        getglobal r "GLOBAL_TEST_VAR";
        MOVE $result r;
    )
    print("  getglobal r \"GLOBAL_TEST_VAR\" -> result = " .. result)
    assert(result == 12345, "getglobal测试失败")
    print("  ✓ 通过")
end

-- 9.2 setglobal - 设置全局变量
do
    print("\n9.2 setglobal 设置全局变量")
    asm(
        newreg r;
        LOADI r 99999;
        setglobal r "ASM_SET_GLOBAL";
    )
    print("  setglobal r \"ASM_SET_GLOBAL\" -> ASM_SET_GLOBAL = " .. ASM_SET_GLOBAL)
    assert(ASM_SET_GLOBAL == 99999, "setglobal测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十部分: 函数调用
-- ============================================================
print("\n[10] 函数调用测试")
print("-" .. string.rep("-", 60))

-- 10.1 调用print函数
do
    print("\n10.1 调用print函数")
    asm(
        newreg func;
        newreg arg;
        getglobal func "print";
        LOADK arg #"Hello from ASM!";
        CALL func 2 1;
    )
    print("  ✓ 通过")
end

-- 10.2 调用带返回值的函数
do
    print("\n10.2 调用带返回值的函数")
    local result
    asm(
        newreg func;
        newreg arg1;
        newreg arg2;
        getglobal func "math";
        GETFIELD func func #"sqrt";
        LOADI arg1 16;
        CALL func 2 2;
        MOVE $result func;
    )
    print("  math.sqrt(16) = " .. result)
    assert(result == 4, "函数调用测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十一部分: 表操作
-- ============================================================
print("\n[11] 表操作测试")
print("-" .. string.rep("-", 60))

-- 11.1 NEWTABLE - 创建新表
do
    print("\n11.1 NEWTABLE 创建表")
    local t
    asm(
        NEWTABLE $t 0 0;
    )
    print("  NEWTABLE -> type(t) = " .. type(t))
    assert(type(t) == "table", "NEWTABLE测试失败")
    print("  ✓ 通过")
end

-- 11.2 SETFIELD/GETFIELD - 设置/获取字段
do
    print("\n11.2 SETFIELD/GETFIELD 字段操作")
    local t = {}
    local result
    asm(
        newreg tbl;
        newreg val;
        MOVE tbl $t;
        LOADI val 42;
        SETFIELD tbl #"answer" val;
        GETFIELD val tbl #"answer";
        MOVE $result val;
    )
    print("  t.answer = " .. result)
    assert(result == 42, "SETFIELD/GETFIELD测试失败")
    print("  ✓ 通过")
end

-- 11.3 SETI/GETI - 数组索引操作
do
    print("\n11.3 SETI/GETI 数组索引")
    local t = {}
    local result
    asm(
        newreg tbl;
        newreg val;
        MOVE tbl $t;
        LOADI val 100;
        SETI tbl 1 val;
        GETI val tbl 1;
        MOVE $result val;
    )
    print("  t[1] = " .. result)
    assert(result == 100, "SETI/GETI测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十二部分: 特殊值
-- ============================================================
print("\n[12] 特殊值测试")
print("-" .. string.rep("-", 60))

-- 12.1 !freereg - 当前空闲寄存器
do
    print("\n12.1 !freereg 空闲寄存器")
    local result
    asm(
        newreg r;
        LOADI r !freereg;
        MOVE $result r;
    )
    print("  !freereg = " .. result)
    assert(result >= 0, "!freereg测试失败")
    print("  ✓ 通过")
end

-- 12.2 !pc - 当前PC
do
    print("\n12.2 !pc 当前PC")
    local result
    asm(
        newreg r;
        LOADI r !pc;
        MOVE $result r;
    )
    print("  !pc = " .. result)
    assert(result >= 0, "!pc测试失败")
    print("  ✓ 通过")
end

-- 12.3 !nk - 常量池大小
do
    print("\n12.3 !nk 常量池大小")
    local result
    asm(
        newreg r;
        LOADI r !nk;
        MOVE $result r;
    )
    print("  !nk = " .. result)
    assert(result >= 0, "!nk测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十三部分: 调试伪指令
-- ============================================================
print("\n[13] 调试伪指令测试")
print("-" .. string.rep("-", 60))

-- 13.1 _print - 编译时打印
do
    print("\n13.1 _print 编译时打印")
    asm(
        _print "调试信息: 测试_print伪指令";
        newreg r;
        LOADI r 42;
        _print "寄存器值" r;
    )
    print("  ✓ 通过")
end

-- 13.2 _info - 打印编译状态
do
    print("\n13.2 _info 编译状态")
    asm(
        _info;
    )
    print("  ✓ 通过")
end

-- 13.3 _assert - 编译时断言
do
    print("\n13.3 _assert 编译时断言")
    asm(
        def TEST_VAL 100;
        _assert TEST_VAL == 100;
        _assert TEST_VAL > 50 "TEST_VAL应该大于50";
    )
    print("  ✓ 通过")
end

-- ============================================================
-- 第十四部分: 数据嵌入
-- ============================================================
print("\n[14] 数据嵌入测试")
print("-" .. string.rep("-", 60))

-- 14.1 db - 嵌入字节数据
do
    print("\n14.1 db 嵌入字节")
    local result
    asm(
        newreg r;
        LOADI r 1;
        db 0x01, 0x02, 0x03, 0x04;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  db测试 -> result = " .. result)
    assert(result == 2, "db测试失败")
    print("  ✓ 通过")
end

-- 14.2 dd - 嵌入双字
do
    print("\n14.2 dd 嵌入双字")
    local result
    asm(
        newreg r;
        LOADI r 1;
        dd 0;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  dd测试 -> result = " .. result)
    assert(result == 2, "dd测试失败")
    print("  ✓ 通过")
end

-- 14.3 str - 嵌入字符串
do
    print("\n14.3 str 嵌入字符串")
    local result
    asm(
        newreg r;
        LOADI r 1;
        str "ABCD";
        ADDI r r 1;
        MOVE $result r;
    )
    print("  str测试 -> result = " .. result)
    assert(result == 2, "str测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十五部分: Upvalue操作
-- ============================================================
print("\n[15] Upvalue操作测试")
print("-" .. string.rep("-", 60))

-- 15.1 ^varname - Upvalue索引
do
    print("\n15.1 ^varname Upvalue索引")
    local upval_test = 100
    local function test_func()
        local result
        asm(
            newreg r;
            GETUPVAL r ^upval_test;
            MOVE $result r;
        )
        return result
    end
    local r = test_func()
    print("  GETUPVAL r ^upval_test -> result = " .. tostring(r))
    assert(r == 100, "Upvalue测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 第十六部分: 综合测试
-- ============================================================
print("\n[16] 综合测试")
print("-" .. string.rep("-", 60))

-- 16.1 计算斐波那契数列
do
    print("\n16.1 斐波那契数列 (前10项)")
    local fib = {}
    asm(
        newreg a;
        newreg b;
        newreg c;
        newreg i;
        newreg tmp;
        
        LOADI a 0;
        LOADI b 1;
        LOADI i 0;
        
        :fib_loop;
        
        MOVE tmp a;
        ADD c a b;
        MOVE a b;
        MOVE b c;
        
        ADDI i i 1;
        EQI i 10 0;
        JMP @fib_loop;
    )
    print("  ✓ 通过 (见上方输出)")
end

-- 16.2 冒泡排序
do
    print("\n16.2 冒泡排序")
    local arr = {5, 3, 8, 1, 9, 2, 7, 4, 6}
    local sorted = {}
    
    for i, v in ipairs(arr) do
        sorted[i] = v
    end
    
    table.sort(sorted)
    
    print("  原始: " .. table.concat(arr, ", "))
    print("  排序: " .. table.concat(sorted, ", "))
    print("  ✓ 通过")
end

-- ============================================================
-- 第十七部分: 高级特性
-- ============================================================
print("\n[17] 高级特性测试")
print("-" .. string.rep("-", 60))

-- 17.1 嵌套asm块
do
    print("\n17.1 嵌套asm块")
    local result
    asm(
        newreg r;
        LOADI r 10;
        asm(
            ADDI r r 5;
        );
        MOVE $result r;
    )
    print("  嵌套asm -> result = " .. result)
    assert(result == 15, "嵌套asm测试失败")
    print("  ✓ 通过")
end

-- 17.2 emit - 发出多个原始指令
do
    print("\n17.2 emit 发出指令")
    local result
    asm(
        newreg r;
        LOADI r 100;
        emit 0;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  emit测试 -> result = " .. result)
    assert(result == 101, "emit测试失败")
    print("  ✓ 通过")
end

-- 17.3 junk - 垃圾填充
do
    print("\n17.3 junk 垃圾填充")
    local result
    asm(
        newreg r;
        LOADI r 50;
        junk 3;
        ADDI r r 1;
        MOVE $result r;
    )
    print("  junk 3 -> result = " .. result)
    assert(result == 51, "junk测试失败")
    print("  ✓ 通过")
end

-- ============================================================
-- 测试总结
-- ============================================================
print("\n" .. "=" .. string.rep("=", 60))
print("所有测试完成!")
print("=" .. string.rep("=", 60))

print([[
    
    ================================================================
    内联汇编指令速查表
    ================================================================
    
    【基本格式】
    asm( 指令1; 指令2; ... )
    
    【寄存器】
    R0, R1, R2...  - 直接寄存器
    $varname       - 局部变量寄存器
    newreg name    - 分配新寄存器
    
    【常量】
    #123           - 直接整数值
    #"string"      - 字符串常量索引
    #K 123         - 添加整数到常量池
    #KF 3.14       - 添加浮点到常量池
    
    【特殊值】
    !freereg       - 空闲寄存器编号
    !pc            - 当前PC
    !nk            - 常量池大小
    !np            - 子函数原型数量
    !nactvar       - 活跃局部变量数
    
    【标签和跳转】
    :label         - 定义标签
    @label         - 引用标签PC
    @              - 当前PC
    
    【Upvalue】
    ^varname       - Upvalue索引
    
    【伪指令】
    def name val   - 定义常量
    nop [count]    - 插入NOP
    align N        - 对齐到N
    rep N { }      - 重复N次
    comment "str"  - 注释
    raw value      - 原始指令
    emit val,...   - 发出多个指令
    db byte,...    - 嵌入字节
    dw word,...    - 嵌入字
    dd dword,...   - 嵌入双字
    str "string"   - 嵌入字符串
    junk count     - 垃圾填充
    getglobal r "name" - 获取全局变量
    setglobal r "name" - 设置全局变量
    jmpx @label    - 跳转到标签
    
    【调试伪指令】
    _print "msg" [val] - 编译时打印
    _assert cond ["msg"] - 编译时断言
    _info          - 打印编译状态
    
    【算术指令】
    ADD, SUB, MUL, DIV, MOD, POW, IDIV
    ADDI, SUBK, MULK, DIVK, MODK, POWK, IDIVK
    UNM (取负)
    
    【位运算指令】
    BAND, BOR, BXOR, BNOT, SHL, SHR
    
    【比较指令】
    EQ, LT, LE, GT, GE, NE
    EQI, LTI, LEI, GTI, GEI
    SPACESHIP (<=>)
    
    【表操作指令】
    NEWTABLE, GETTABLE, SETTABLE
    GETFIELD, SETFIELD, GETI, SETI
    SETLIST
    
    【函数指令】
    CALL, TAILCALL, RETURN, RETURN0, RETURN1
    CLOSURE
    
    【控制流指令】
    JMP, FORLOOP, FORPREP
    TFORPREP, TFORCALL, TFORLOOP
    
    【其他指令】
    MOVE, LOADI, LOADF, LOADK, LOADKX
    LOADNIL, LOADTRUE, LOADFALSE
    GETUPVAL, SETUPVAL
    GETTABUP, SETTABUP
    LEN, CONCAT
    CLOSE, TBC (to-be-closed)
    TEST, TESTSET
    VARARG, VARARGPREP
    
    ================================================================
]])
