local tcc = require("tcc")

function test_pure_c_arithmetic()
    print("Testing Pure C Arithmetic Generation...")

    local code = [[
        local a = 10
        local b = 20
        return a + b
    ]]

    -- Compile with pure_c flag (2nd argument as boolean)
    local c_code = tcc.compile(code, true)

    -- Check for absence of lua_arith call
    if string.find(c_code, "lua_arith%(") then
        print(c_code)
        error("Pure C mode failed: Found 'lua_arith(' call in generated code for ADD operation.")
    end

    -- Check for presence of C addition
    -- The expected code structure for "return a + b" in pure C mode:
    -- lua_pushnumber(L, lua_tonumber(L, ...) + lua_tonumber(L, ...));
    if not string.find(c_code, "+") then
        error("Pure C mode failed: Did not find '+' operator in generated code.")
    end

    print("test_pure_c_arithmetic passed")
end

function test_pure_c_bitwise()
    print("Testing Pure C Bitwise Generation...")

    local code = [[
        local a = 10
        local b = 20
        return a & b
    ]]

    local c_code = tcc.compile(code, true)

    if string.find(c_code, "lua_arith%(") then
        error("Pure C mode failed: Found 'lua_arith(' call in generated code for BAND operation.")
    end

    if not string.find(c_code, "&") then
        error("Pure C mode failed: Did not find '&' operator in generated code.")
    end

    print("test_pure_c_bitwise passed")
end

function test_pure_c_mixed()
    print("Testing Pure C Mixed Generation...")

    local code = [[
        local x = 10
        return -x * 2
    ]]

    local c_code = tcc.compile(code, true)

    if string.find(c_code, "lua_arith%(") then
        error("Pure C mode failed: Found 'lua_arith(' call in generated code for UNM/MUL.")
    end

    print("test_pure_c_mixed passed")
end

-- Run tests
test_pure_c_arithmetic()
test_pure_c_bitwise()
test_pure_c_mixed()

print("All Pure C tests passed!")
