-- 测试函数参数默认值语法糖

print("=== 测试1: 基本常量默认值 ===")
function greet(name = "World", greeting = "Hello")
    return greeting .. ", " .. name .. "!"
end
print(greet())              -- 期望: Hello, World!
print(greet("Lua"))         -- 期望: Hello, Lua!
print(greet("Lua", "Hi"))   -- 期望: Hi, Lua!

print("\n=== 测试2: 数值默认值 ===")
function add(a = 0, b = 0)
    return a + b
end
print(add())        -- 期望: 0
print(add(5))       -- 期望: 5
print(add(3, 7))    -- 期望: 10

print("\n=== 测试3: 默认值引用前面的参数 ===")
function make_range(start = 1, stop = start * 10, step = 1)
    local result = {}
    for i = start, stop, step do
        result[#result + 1] = i
    end
    return result
end
local r1 = make_range()
print("range():", table.concat(r1, ","))  -- 期望: 1,2,3,4,5,6,7,8,9,10

local r2 = make_range(5)
print("range(5):", table.concat(r2, ","))  -- 期望: 5,6,...,50

print("\n=== 测试4: nil显式传入触发默认值 ===")
function foo(x = 42)
    return x
end
print(foo(nil))   -- 期望: 42 (nil触发默认值)
print(foo(false)) -- 期望: false (false不触发默认值)
print(foo(0))     -- 期望: 0 (0不触发默认值)

print("\n=== 测试5: 混合有无默认值的参数 ===")
function mixed(a, b = 10, c, d = 20)
    return a, b, c, d
end
print(mixed(1, 2, 3, 4))       -- 期望: 1 2 3 4
print(mixed(1, nil, 3))         -- 期望: 1 10 3 20
print(mixed(1))                 -- 期望: 1 10 nil 20

print("\n=== 测试6: 表达式默认值 ===")
local counter = 0
function next_id()
    counter = counter + 1
    return counter
end

function create_item(name = "item_" .. tostring(next_id()), value = {})
    return {name = name, value = value}
end

local item1 = create_item()
local item2 = create_item()
local item3 = create_item("custom")
print("item1:", item1.name)   -- 期望: item_1
print("item2:", item2.name)   -- 期望: item_2
print("item3:", item3.name)   -- 期望: custom

print("\n=== 测试8: 箭头函数默认值 ===")
local div = =>(a = 10, b = 2){ a / b }
print(div())        -- 期望: 5.0
print(div(20))      -- 期望: 10.0
print(div(20, 4))   -- 期望: 5.0

print("\n=== 测试9: 方法中的默认值 ===")
local obj = {}
function obj:method(x = 100)
    return x
end
print(obj:method())     -- 期望: 100
print(obj:method(999))  -- 期望: 999

print("\n=== 测试10: vararg与默认值共存 ===")
function vfunc(a = 1, b = 2, ...)
    return a, b, ...
end
print(vfunc())            -- 期望: 1 2
print(vfunc(10, 20, 30))  -- 期望: 10 20 30

print("\n所有测试完成!")
