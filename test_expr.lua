local m_expr = math.toexpr(42, 2)
print("Math expr:", m_expr)
local f = load("return " .. m_expr)
local f_res = f()
assert(math.abs(f_res - 42) < 0.001, "math.toexpr mismatch: " .. tostring(f_res))

local b_expr = bool.toexpr(true, 3)
print("Bool expr:", b_expr)
local fb = load("return " .. b_expr)
assert(fb() == true, "bool.toexpr mismatch")

local b_expr2 = bool.toexpr(false, 3)
print("Bool expr2:", b_expr2)
local fb2 = load("return " .. b_expr2)
assert(fb2() == false, "bool.toexpr mismatch")

print("All OK")
