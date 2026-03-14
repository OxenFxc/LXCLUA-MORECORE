-- tests/test_custom_opcodes_debug.lua

local function assert_eq(a, b, msg)
    if a ~= b then
        error(string.format("Assertion failed: %s (expected %s, got %s)", msg or "", tostring(b), tostring(a)))
    end
end

print("Testing custom 64-bit opcodes interaction with debug hooks...")

local hook_count = 0
local function hookf(event, line)
    hook_count = hook_count + 1
end

-- A function that will use the ADDI extended instruction (assuming A += B where B is an immediate is emitted for `x = x + 10`)
local function test_func(x)
    local y = x + 10 -- ADDI
    local z = y * 2  -- MULK
    return z
end

-- 1. Test running it without hook
local res1 = test_func(5)
assert_eq(res1, 30, "test_func normal execution")

-- 2. Test running it with line hook
debug.sethook(hookf, "l")
local res2 = test_func(5)
debug.sethook() -- remove hook

assert_eq(res2, 30, "test_func execution with line hook")
if hook_count == 0 then
    error("Hook was not called during execution!")
end

print("Hook was called " .. hook_count .. " times.")

-- 3. Test running with instruction hook
local inst_hook_count = 0
local function inst_hookf(event)
    inst_hook_count = inst_hook_count + 1
end

debug.sethook(inst_hookf, "", 1) -- Call hook every 1 instruction
local res3 = test_func(5)
debug.sethook()

assert_eq(res3, 30, "test_func execution with instruction hook")
if inst_hook_count == 0 then
    error("Instruction hook was not called during execution!")
end

print("Instruction hook was called " .. inst_hook_count .. " times.")
print("All debug hook tests with extended opcodes passed.")
