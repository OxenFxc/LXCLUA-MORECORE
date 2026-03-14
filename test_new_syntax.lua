print("=== Array Destruct ===")
local arr = {10, 20, 30}
local take [first, , third] = arr
assert(first == 10)
assert(third == 30)

print("=== Dict Destruct ===")
local dict = {a=1, b=2}
local take {a, b} = dict
assert(a == 1)
assert(b == 2)

print("=== Spread Op ===")
local arr1 = {1, 2}
local b = { 0, ...arr1 }
assert(b[1] == 0)
assert(b[2] == 1)
assert(b[3] == 2)

local function sum(...)
   local args = {...}
   local s = 0
   for i=1, #args do s = s + args[i] end
   return s
end
assert(sum(1, ...arr1) == 4)

print("=== Continue ===")
local s = 0
for i=1,10 do
   if i % 2 == 0 then continue end
   s = s + i
end
assert(s == 25)

print("=== Default Param ===")
function default_greet(name = "World") return name end
assert(default_greet() == "World")
assert(default_greet("Lua") == "Lua")

print("=== Ternary ===")
local is_debug = true
assert((is_debug ? 1 : 0) == 1)

print("=== Comprehensions ===")
local evens = [for _, v in ipairs({1,2,3,4}) do v * 2 if v % 2 == 0]
assert(evens[1] == 4)
assert(evens[2] == 8)

print("ALL PASSED!")
