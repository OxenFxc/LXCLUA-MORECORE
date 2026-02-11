
print("Checking _G keys:")
for k,v in pairs(_G) do
    if k:find("fenv") then
        print("Found:", k, v)
    end
end

if not getfenv then
    print("getfenv is nil!")
else
    print("getfenv is present")
end

print("_ENV == _G", _ENV == _G)
print("_ENV:", _ENV)
print("_G:", _G)

local function assert_eq(a, b, msg)
    if a ~= b then
        error(msg .. ": " .. tostring(a) .. " ~= " .. tostring(b))
    end
end

print("Testing getfenv/setfenv...")

print("type(getfenv) before call:", type(getfenv))

-- Try indirect call
local my_getfenv = getfenv
print("Calling local alias...")
local g = my_getfenv(0)
assert_eq(g, _G, "local alias getfenv(0) should return _G")
print("Local alias worked.")

-- 1. Test getfenv(0)
print("Calling global direct...")
local g2 = getfenv(0)
assert_eq(g2, _G, "getfenv(0) should return _G")
print("Global direct worked.")

-- Check if getfenv is still there
if getfenv == nil then
    print("getfenv disappeared!")
else
    print("getfenv is still here:", getfenv)
end

-- 2. Test getfenv(1)
print("Calling getfenv(1)...")
local f_env = getfenv(1)
assert_eq(f_env, _G, "getfenv(1) should return _G initially")
print("getfenv(1) worked.")

-- 3. Test setfenv on a standard function with _ENV
local function test_env()
    return _ENV.x
end

local env1 = {x = 10}
setfenv(test_env, env1)
assert_eq(getfenv(test_env), env1, "getfenv should return new env")
assert_eq(test_env(), 10, "function should use new env")

-- 4. Test setfenv on current function (1)
local env2 = {x = 20}

-- Prepare env2 globals BEFORE setfenv
env2.print = print
env2.error = error
env2.tostring = tostring
env2.getfenv = getfenv
env2.assert_eq = assert_eq
env2.type = type
env2.pairs = pairs
env2.pcall = pcall
env2.setfenv = setfenv

print("Prepared env2.print:", env2.print)

setfenv(1, env2)
-- Note: setfenv(1) changes the environment of the *calling* function?
-- In 5.1, setfenv(1, t) changes the env of the function running setfenv.
-- Wait, setfenv(1) changes the environment of the current function.
-- So if I do setfenv(1, env2), subsequent global accesses in THIS chunk should use env2.

-- Let's test if globals are resolved from env2 now.

-- Verify x
if x ~= 20 then
    -- error is in env2
    error("setfenv(1) failed to change environment: x is " .. tostring(x))
end

-- Verify print availability
if not print then
    -- If print is nil, we can't print error!
    -- But error is also global.
    -- Hopefully error works?
    -- If error fails, we crash with "attempt to call nil value".
    if env2.error then
       env2.error("print is nil in new env!")
    else
       -- Hard crash
       local f = nil
       f()
    end
end

print("setfenv(1) works.")

-- Restore _G for safety (though this script ends soon)
-- We can't easily restore _G if we don't have reference to it, but we stored env2 which doesn't have it.
-- But 'getfenv' is in env2.
-- But we can't get _G unless we saved it.
local old_G = g
-- But we are in env2. We can't access 'old_G' unless it's an upvalue or in env2.
-- It is a local variable 'g' and 'old_G', so they are upvalues/registers. We can access them.
-- setfenv(1, old_G)

-- 5. Test setfenv(0)
-- This is likely to fail based on my reading of the code.
local status, err = pcall(function()
    setfenv(0, {})
end)

if not status then
    print("setfenv(0) failed as expected (or maybe implemented now?): " .. tostring(err))
    -- If we implemented setfenv(0), it should succeed or return nil.
    -- If it errored, check why.
else
    print("setfenv(0) succeeded.")
end

-- 6. Test function without _ENV
-- A function that doesn't use globals might not have _ENV upvalue.
local function no_globals()
    local a = 1
    return a
end

local env3 = {a=2}
local ret = setfenv(no_globals, env3)
-- In 5.1, this would attach env3. In 5.2 emulation, it might fail to attach if optimized.
-- If it returns ret, let's see what getfenv says.
local current = getfenv(no_globals)
if current == env3 then
    print("setfenv on no-global function worked (unexpected for emulation?)")
else
    print("setfenv on no-global function did not attach env (expected for emulation)")
end

print("Done.")
