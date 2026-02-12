local verbose = true
local function log(...)
  if verbose then print(...) end
end

log("Starting VM Protection test...")

local function test_func(a, b)
  local c = a + b
  local d = c * 2
  local e = d - 5
  local f = e // 2
  local g = f & 0x0F
  return g
end

-- Verify original function
local res = test_func(10, 20)
-- 10+20=30; 30*2=60; 60-5=55; 55//2=27; 27&15=11
assert(res == 11, "Original function check failed: " .. tostring(res))
log("Original function OK")

-- Dump with VM Protection
local dump_opts = {
  strip = true,
  obfuscate = 128, -- OBFUSCATE_VM_PROTECT
  envelop = false -- simpler
}

local bytecode = string.dump(test_func, dump_opts)
log("Dumped bytecode size: " .. #bytecode)

-- Load the bytecode
local loaded_func, err = load(bytecode)
if not loaded_func then
  log("Failed to load bytecode: " .. tostring(err))
  os.exit(1)
end

-- Execute loaded function
local res_vm = loaded_func(10, 20)
assert(res_vm == 11, "VM Protected function check failed: " .. tostring(res_vm))
log("VM Protected function OK")

-- Test metamethods in VM Protected code
local function test_meta()
  local mt = {
    __add = function(a, b) return a.v + b.v end
  }
  local t1 = setmetatable({v=10}, mt)
  local t2 = setmetatable({v=20}, mt)
  return t1 + t2
end

local bytecode_meta = string.dump(test_meta, dump_opts)
local loaded_meta, err_meta = load(bytecode_meta)
if not loaded_meta then
  log("Failed to load meta bytecode: " .. tostring(err_meta))
  os.exit(1)
end
local res_meta = loaded_meta()
assert(res_meta == 30, "VM Protected metamethod check failed: " .. tostring(res_meta))
log("VM Protected metamethod OK")

log("All VM Protection tests passed!")
