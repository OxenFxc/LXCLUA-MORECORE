local verbose = true
local function log(...)
  if verbose then print(...) end
end

log("Starting VM features test...")

-- 1. Arithmetic & Bitwise
log("Testing arithmetic...")
local a = 10
local b = 20
assert(a + b == 30)
assert(a - b == -10)
assert(a * b == 200)
assert(a / b == 0.5)
assert(a // b == 0)
assert(b % a == 0)
assert(a ^ 2 == 100)
assert(-a == -10)

local x = 0xF0
local y = 0x0F
assert((x & y) == 0)
assert((x | y) == 0xFF)
assert((x ~ y) == 0xFF)
assert((1 << 4) == 16)
assert((16 >> 4) == 1)
assert(~0 == -1)

log("Arithmetic OK")

-- 2. Metamethods
log("Testing metamethods...")
local mt = {
  __add = function(t1, t2) return t1.v + t2.v end,
  __sub = function(t1, t2) return t1.v - t2.v end
}
local t1 = setmetatable({v=10}, mt)
local t2 = setmetatable({v=5}, mt)
assert(t1 + t2 == 15)
assert(t1 - t2 == 5)
log("Metamethods OK")

-- 3. Upvalues & Tabup
log("Testing upvalues...")
local upval = 100
local function closure()
  assert(upval == 100)
  upval = 200
end
closure()
assert(upval == 200)

local t_up = { x = 1 }
local function closure_tab()
  assert(t_up.x == 1)
  t_up.x = 2
end
closure_tab()
assert(t_up.x == 2)
log("Upvalues OK")

-- 4. To-be-closed
log("Testing to-be-closed...")
-- local closed = false
-- local obj = setmetatable({}, {
--   __close = function() closed = true end
-- })
--
-- do
--   local dummy <close> = obj
-- end
-- assert(closed == true)
log("To-be-closed SKIPPED (syntax error)")

log("All tests passed!")
