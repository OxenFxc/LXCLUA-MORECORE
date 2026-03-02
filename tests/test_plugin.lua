local code = [[
PRINT "Initializing plugin testing..."
SET_GSTR "PLUGIN_TEST_STR" "test_value"
SET_GINT "PLUGIN_TEST_INT" 42
RUN "print('Checking dynamic Lua run via RUN command...')"
DUMP_STACK
ADD_OPCODE "CUSTOM_OP" "0xFE"
GC
RUN "print('Garbage collection triggered via GC command.')"
]]

plugin.load(code)

assert(PLUGIN_TEST_STR == "test_value", "SET_GSTR failed!")
assert(PLUGIN_TEST_INT == 42, "SET_GINT failed!")

local opcodes = debug.getregistry()["PLUGIN_OPCODES"]
assert(opcodes["CUSTOM_OP"] == "0xFE", "ADD_OPCODE failed!")

print("All base plugin tests passed!")

function my_test_func(x)
    return x * 2
end

function my_hook_func(x)
    print("Hook called with:", x)
end

local hook_code = [[
HOOK_FUNC "my_test_func" "my_hook_func"
]]

plugin.load(hook_code)
local result = my_test_func(10)
assert(result == 20, "HOOK_FUNC failed to preserve original return value!")

function my_replace_func(x)
    return x * 10
end

local replace_code = [[
REPLACE_FUNC "my_test_func" "my_replace_func"
]]

plugin.load(replace_code)
local replace_result = my_test_func(5)
assert(replace_result == 50, "REPLACE_FUNC failed!")

print("All plugin tests passed successfully!")
