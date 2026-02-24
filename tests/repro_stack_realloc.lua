-- tests/repro_stack_realloc.lua
print("Running Stack Reallocation Reproduction Test...")

local function recurse(depth)
    if depth > 0 then
        local a, b, c, d, e = 1, 2, 3, 4, 5
        return recurse(depth - 1) + a
    else
        return 0
    end
end

-- Function to create garbage that triggers stack growth on collection
local function create_garbage()
    for i = 1, 50 do
        _G["defer"](function()
            recurse(200)
        end)
    end
end

function Gen(T)(val)
    return { type = T, value = val }
end

collectgarbage("stop")

print("Starting loop...")
for i = 1, 1000 do
    create_garbage()

    -- Manually drive GC step to try and trigger collection during next allocation
    collectgarbage("step", 5)

    -- Call Generic Function (OP_GENERICWRAP)
    -- Use "number" because "int" is not recognized as specialization in C generic wrapper
    local obj = Gen("number")(i)

    if obj.value ~= i then
        error("Value mismatch!")
    end
end

print("Stack Reallocation Reproduction Test Passed (No Crash).")
