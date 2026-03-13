-- ============================================================
-- 展开运算符 (...) 换行行为 诊断测试
-- 每个case都打印实际值，用于定位bug根因
-- ============================================================

local passed = 0
local failed = 0
local total = 0

--- 断言辅助函数，失败时打印实际值
--- @param cond boolean 断言条件
--- @param msg string 测试描述
local function check(cond, msg)
    total = total + 1
    if cond then
        passed = passed + 1
        print("[PASS] " .. msg)
    else
        failed = failed + 1
        print("[FAIL] " .. msg)
    end
end

--- 将table的完整内容转为字符串（含数组部分和哈希部分）
--- @param t table 要dump的表
--- @param indent string 缩进层级
--- @return string dump结果
local function dump(t, indent)
    if type(t) ~= "table" then return tostring(t) end
    indent = indent or ""
    local parts = {}
    -- 数组部分
    for i = 1, #t do
        parts[#parts+1] = indent .. "  [" .. i .. "] = " .. tostring(t[i])
    end
    -- 哈希部分（跳过数组索引）
    for k, v in pairs(t) do
        if type(k) ~= "number" or k < 1 or k > #t or k ~= math.floor(k) then
            parts[#parts+1] = indent .. "  [" .. tostring(k) .. "] = " .. tostring(v)
        end
    end
    if #parts == 0 then return "{}" end
    return "{\n" .. table.concat(parts, "\n") .. "\n" .. indent .. "}"
end

--- rawlen 版 dump，检查 0 和负索引等边角
--- @param t table 要dump的表
--- @return string dump结果
local function dump_raw(t)
    if type(t) ~= "table" then return tostring(t) end
    local parts = {}
    -- 检查 0 索引
    if rawget(t, 0) ~= nil then
        parts[#parts+1] = "  [0] = " .. tostring(rawget(t, 0))
    end
    -- 数组部分 1..rawlen
    local n = rawlen(t)
    parts[#parts+1] = "  #t = " .. tostring(n)
    for i = 1, n do
        parts[#parts+1] = "  [" .. i .. "] = " .. tostring(rawget(t, i)) .. "  (type=" .. type(rawget(t, i)) .. ")"
    end
    -- 查看 rawlen 之后有没有空洞元素
    for i = n+1, n+5 do
        local v = rawget(t, i)
        if v ~= nil then
            parts[#parts+1] = "  [" .. i .. "] = " .. tostring(v) .. "  (超出#t!)"
        end
    end
    -- 哈希部分
    for k, v in pairs(t) do
        if type(k) ~= "number" or k < 0 or k > n + 5 or k ~= math.floor(k) then
            parts[#parts+1] = "  [" .. tostring(k) .. "] = " .. tostring(v)
        end
    end
    return "{\n" .. table.concat(parts, "\n") .. "\n}"
end

--- 收集所有参数并返回为table
--- @param ... any 可变参数
--- @return table 所有参数组成的列表
local function collect(...)
    local n = select("#", ...)
    local t = {}
    for i = 1, n do
        t[i] = select(i, ...)
    end
    return t, n
end

print("========== 展开运算符 诊断测试 ==========\n")

-- ============================================================
-- 第一组: 函数调用中的展开运算符 (simpleexp路径, table.unpack)
-- ============================================================
print("--- 1. 函数调用中的展开 (simpleexp 路径) ---")

local t1 = {10, 20, 30}
local r1, n1 = collect(...t1)
print("  collect(...t1): n=" .. n1 .. " 值=" .. dump(r1))
check(n1 == 3 and r1[1] == 10 and r1[2] == 20 and r1[3] == 30,
    "collect(...t1) => {10,20,30}")

local r2, n2 = collect(...{40, 50, 60})
print("  collect(...{40,50,60}): n=" .. n2 .. " 值=" .. dump(r2))
check(n2 == 3 and r2[1] == 40 and r2[2] == 50 and r2[3] == 60,
    "collect(...{literal}) => {40,50,60}")

print("")

-- ============================================================
-- 第二组: 表构造器中的展开 (constructor 路径, ipairs循环)
-- ============================================================
print("--- 2. 表构造器中的展开 (constructor 路径) ---")

print("\n  -- 2.1: {100, ...src, 200} where src={1,2,3} --")
local src = {1, 2, 3}
local r4 = {100, ...src, 200}
print("  实际结果:")
print(dump_raw(r4))
check(r4[1] == 100, "r4[1]==100, 实际=" .. tostring(r4[1]))
check(r4[2] == 1, "r4[2]==1, 实际=" .. tostring(r4[2]))
check(r4[3] == 2, "r4[3]==2, 实际=" .. tostring(r4[3]))
check(r4[4] == 3, "r4[4]==3, 实际=" .. tostring(r4[4]))
check(r4[5] == 200, "r4[5]==200, 实际=" .. tostring(r4[5]))

print("\n  -- 2.2: {...src} 纯展开 (非vararg函数) --")
local r4b = {...src}
print("  实际结果:")
print(dump_raw(r4b))
check(r4b[1] == 1 and r4b[2] == 2 and r4b[3] == 3,
    "{...src}=>{1,2,3}, 实际[1]=" .. tostring(r4b[1]) .. ",[2]=" .. tostring(r4b[2]) .. ",[3]=" .. tostring(r4b[3]))

print("\n  -- 2.3: {...a, ...b} 多重展开, a={1,2}, b={3,4} --")
local a = {1, 2}
local b = {3, 4}
local r5 = {...a, ...b}
print("  实际结果:")
print(dump_raw(r5))
check(r5[1] == 1 and r5[2] == 2 and r5[3] == 3 and r5[4] == 4,
    "{...a,...b}=>{1,2,3,4}")

print("\n  -- 2.4: {99, ...empty, 100} 展开空表 --")
local empty = {}
local r6 = {99, ...empty, 100}
print("  实际结果:")
print(dump_raw(r6))
check(r6[1] == 99, "r6[1]==99, 实际=" .. tostring(r6[1]))
check(r6[2] == 100, "r6[2]==100, 实际=" .. tostring(r6[2]))

print("\n  -- 2.5: {x='hello', ...mix, y='world'} 混合展开 --")
local mix_src = {10, 20}
local r_mix = {x = "hello", ...mix_src, y = "world"}
print("  实际结果:")
print(dump_raw(r_mix))
print("  x=" .. tostring(r_mix.x) .. " y=" .. tostring(r_mix.y))
check(r_mix.x == "hello", "混合: x='hello', 实际=" .. tostring(r_mix.x))
check(r_mix.y == "world", "混合: y='world', 实际=" .. tostring(r_mix.y))
check(r_mix[1] == 10, "混合: [1]==10, 实际=" .. tostring(r_mix[1]))
check(r_mix[2] == 20, "混合: [2]==20, 实际=" .. tostring(r_mix[2]))

print("")

-- ============================================================
-- 第三组: vararg函数内 展开 vs varargs 换行区分 (关键!)
-- ============================================================
print("--- 3. vararg函数内 展开 vs varargs 换行区分 ---")

-- 3.1 vararg函数: ... 后换行 → 应该是varargs
print("\n  -- 3.1: 赋值语句中 ...\\n → varargs --")
local function va_newline(...)
    local x =
...
    return x
end
local r7 = va_newline(42)
print("  va_newline(42) => " .. tostring(r7))
check(r7 == 42, "...换行 => varargs, r7==42")

-- 3.2 vararg函数: 表构造器中 {...t} 同行
print("\n  -- 3.2: vararg函数中 {...t} 同行 --")
local function va_spread_table(...)
    local t = {100, 200, 300}
    local r = {...t}
    return r
end
local r8 = va_spread_table(1, 2, 3)
print("  va_spread_table(1,2,3) 中 {...t} 实际结果:")
print(dump_raw(r8))
-- 表构造器中 ... 后跟 t(TK_NAME), 不是 ,;} → 走展开路径
-- 预期: {100,200,300} (展开t), 不是 {1,2,3} (varargs)
check(r8[1] == 100, "表构造器{...t}: [1]==100(展开t), 实际=" .. tostring(r8[1]))

-- 3.3 vararg函数: 表构造器中 跨行
print("\n  -- 3.3: vararg函数中 {...\\nt} 跨行 --")
local function va_spread_newline(...)
    local t = {10, 20}
    local r = {
...
,t}
    return r
end
local r9 = va_spread_newline("a", "b")
print("  va_spread_newline('a','b') 实际结果:")
print(dump_raw(r9))
-- ... 后换行, 在表构造器中 lookahead 是换行后的 token
-- 但表构造器不检查行号! 它只看 lookahead 是不是 ,;}
-- ... 后面下一行是空行,然后是 ,t}
-- 所以 lookahead 是 ',' → 走varargs路径
-- 预期: {"a","b",{10,20}}
check(type(r9[1]) == "string" and r9[1] == "a",
    "跨行: [1]=='a', 实际=" .. tostring(r9[1]))

-- 3.4 vararg函数: 函数调用中 ...t 同行
print("\n  -- 3.4: vararg函数中 f(...t) 同行 --")
local function va_spread_call(...)
    local t = {100, 200}
    return collect(...t)
end
local r10, n10 = va_spread_call(1, 2, 3)
print("  va_spread_call(1,2,3) 中 collect(...t) 实际结果: n=" .. n10)
print(dump_raw(r10))
-- simpleexp路径: ... 后面跟 t(TK_NAME) 且同行 → table.unpack(t)
-- 预期: {100,200}
check(r10[1] == 100 and r10[2] == 200,
    "函数调用...t: => {100,200}(展开t)")

-- 3.5 vararg函数: 函数调用中 ...\\n name 跨行
print("\n  -- 3.5: vararg函数中 f(...)\\n → varargs --")
local function va_vararg_call(...)
    local r, n = collect(
...
)
    return r, n
end
local r11, n11 = va_vararg_call(7, 8, 9)
print("  va_vararg_call(7,8,9) 中 collect(...)换行 实际结果: n=" .. n11)
print(dump_raw(r11))
check(r11[1] == 7 and r11[2] == 8 and r11[3] == 9,
    "函数调用...换行 => varargs {7,8,9}")

print("")

-- ============================================================
-- 第四组: 函数参数中的展开位置
-- ============================================================
print("--- 4. 函数参数展开位置 ---")

-- 4.1 末尾展开 (保留全部返回值)
print("\n  -- 4.1: collect(0, ...t_end) 末尾展开 --")
local t_end = {1, 2, 3}
local r12, n12 = collect(0, ...t_end)
print("  n=" .. n12 .. " 值=" .. dump(r12))
check(r12[1] == 0 and r12[2] == 1 and r12[3] == 2 and r12[4] == 3,
    "末尾展开: {0,1,2,3}")

-- 4.2 非末尾展开 (只取第一个值)
print("\n  -- 4.2: three_args(...mid, 3) 中间展开 --")
local function three_args(a, b, c)
    return a, b, c
end
local mid = {1, 2}
local a2, b2, c2 = three_args(...mid, 3)
print("  a=" .. tostring(a2) .. " b=" .. tostring(b2) .. " c=" .. tostring(c2))
-- ...mid → table.unpack(mid) → 1, 2 (多返回值)
-- 但在非末尾位置, table.unpack(mid) 只保留第一个值=1
-- 所以参数是 (1, 3) → a=1, b=3, c=nil
check(a2 == 1, "中间展开: a=1, 实际=" .. tostring(a2))
check(b2 == 3, "中间展开: b=3(非末尾截断), 实际=" .. tostring(b2))
check(c2 == nil, "中间展开: c=nil, 实际=" .. tostring(c2))

-- 4.3 展开后紧跟其他参数
print("\n  -- 4.3: collect(...t3, 99) --")
local t3 = {1, 2}
local r13, n13 = collect(...t3, 99)
print("  n=" .. n13 .. " 值=" .. dump(r13))
-- ...t3 非末尾 → 只取第一个值=1, 然后99
-- 预期: {1, 99}
check(r13[1] == 1, "非末尾展开: [1]=1, 实际=" .. tostring(r13[1]))
check(r13[2] == 99, "非末尾展开: [2]=99, 实际=" .. tostring(r13[2]))

print("")

-- ============================================================
-- 第五组: 边界条件
-- ============================================================
print("--- 5. 边界条件 ---")

-- 5.1 展开嵌套table
print("\n  -- 5.1: collect(...nested) 嵌套表 --")
local nested = {{1, 2}, {3, 4}}
local r14, n14 = collect(...nested)
print("  n=" .. n14 .. " [1]=" .. tostring(r14[1]) .. " [2]=" .. tostring(r14[2]))
check(type(r14[1]) == "table" and r14[1][1] == 1, "嵌套[1]={1,2}")
check(type(r14[2]) == "table" and r14[2][1] == 3, "嵌套[2]={3,4}")

-- 5.2 展开单元素
print("\n  -- 5.2: collect(...single) 单元素 --")
local single = {999}
local r15, n15 = collect(...single)
print("  n=" .. n15 .. " [1]=" .. tostring(r15[1]))
check(r15[1] == 999 and n15 == 1, "单元素: {999}")

-- 5.3 展开空表 (函数调用)
print("\n  -- 5.3: collect(...{}) 空表展开 --")
local r16, n16 = collect(...{})
print("  n=" .. n16)
check(n16 == 0, "空表展开: 0个参数, 实际n=" .. n16)

print("")

-- ============================================================
-- 结果汇总
-- ============================================================
print("========================================")
print(string.format("测试结果: %d/%d 通过, %d 失败", passed, total, failed))
print("========================================")

if failed > 0 then
    print("!!! 存在失败的测试 !!!")
else
    print("所有测试通过!")
end
