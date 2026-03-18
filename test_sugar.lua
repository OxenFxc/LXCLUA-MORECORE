print("=== testing range loop ===")
local sum = 0
for i in 1..10 do
    sum = sum + i
end
assert(sum == 55)
print("range loop OK")

print("=== testing exponent ===")
local result = 2 ** 3
assert(result == 8)
print("exponent OK")

print("=== testing in-place swap ===")
local a, b = 1, 2
a >< b
assert(a == 2 and b == 1)

local t = {x=10, y=20}
t.x >< t.y
assert(t.x == 20 and t.y == 10)
print("in-place swap OK")

print("=== ALL PASSED ===")
