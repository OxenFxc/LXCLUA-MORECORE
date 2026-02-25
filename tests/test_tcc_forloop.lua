local tcc = require("tcc")

local function compile_and_load(name, code, use_pure_c)
    print("Compiling " .. name .. "...")
    local c_code = tcc.compile(code, name, use_pure_c)
    local c_file = name .. ".c"
    local so_file = name .. ".so"

    local f = io.open(c_file, "w")
    f:write(c_code)
    f:close()

    local cmd = string.format("gcc -std=c99 -shared -o %s %s -I. -fPIC", so_file, c_file)
    local ret = os.execute(cmd)
    if ret ~= 0 and ret ~= true then
        error("GCC compilation failed for " .. name)
    end

    if package.loaded[name] then
        package.loaded[name] = nil
    end

    local old_path = package.cpath
    package.cpath = "./?.so;" .. old_path
    local ok, mod = pcall(require, name)
    package.cpath = old_path

    if not ok then
        error("Failed to load module " .. name .. ": " .. tostring(mod))
    end
    return mod
end

-- Test Case: Pairs Loop
-- This tests OP_TFORCALL and OP_TFORLOOP logic.
-- If OP_TFORLOOP fails to update the control variable, this will infinite loop.
local code = [[
    local function test_pairs()
        local t = {a=1, b=2, c=3}
        local count = 0
        local keys = {}
        for k,v in pairs(t) do
            count = count + 1
            keys[k] = true
            if count > 10 then
                return "infinite loop"
            end
        end
        return count
    end
    return test_pairs()
]]

print("Running reproduction test...")
local result = compile_and_load("repro_forloop", code)

print("Result: " .. tostring(result))

if result == "infinite loop" then
    print("FAILURE: Infinite loop detected (Bug reproduced)")
    os.exit(1)
elseif result == 3 then
    print("SUCCESS: Loop terminated correctly")
    os.exit(0)
else
    print("FAILURE: Unexpected result: " .. tostring(result))
    os.exit(1)
end
