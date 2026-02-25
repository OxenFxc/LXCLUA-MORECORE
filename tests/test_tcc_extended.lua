local tcc = require("tcc")

local function verify(name, func)
    print("Verifying " .. name .. "...")
    local bytecode = string.dump(func)
    local status, c_code = pcall(tcc.compile, bytecode, name)

    if not status then
        print("  Error calling tcc.compile: " .. tostring(c_code))
        return
    end

    if c_code:find("/%* Unimplemented opcode:") then
        print("  FAIL: Contains unimplemented opcodes.")
        for line in c_code:gmatch("[^\r\n]+") do
            if line:find("Unimplemented opcode") then
                print("    " .. line:match("/%* (.*) %*/"))
            end
        end
    else
        print("  PASS: No unimplemented opcodes found.")
    end
    -- print(c_code) -- Debug
end

verify("Tables", function()
    local t = {}
    t.x = 10
    t["y"] = 20
    local z = t.x + t.y
    return z
end)

verify("Loops", function()
    local sum = 0
    for i = 1, 10 do
        sum = sum + i
    end
    return sum
end)

verify("Logic", function()
    local a = true
    local b = false
    if not b then
        return a
    end
end)
