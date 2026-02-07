print("--- Starting Test ---")

-- 1. Simple if-else
local function test_if(x)
    if x > 10 then
        return "greater than 10"
    else
        return "less or equal to 10"
    end
end
print("Test If (15):", test_if(15))
print("Test If (5):", test_if(5))

-- 2. While loop
local function test_while(n)
    local sum = 0
    local i = 1
    while i <= n do
        sum = sum + i
        i = i + 1
    end
    return sum
end
print("Test While (10):", test_while(10))

-- 3. For loop
local function test_for(n)
    local sum = 0
    for i = 1, n do
        sum = sum + i
    end
    return sum
end
print("Test For (10):", test_for(10))

-- 4. Generic for loop
local function test_generic_for()
    local t = {10, 20, 30}
    local sum = 0
    for i, v in ipairs(t) do
        sum = sum + v
    end
    return sum
end
print("Test Generic For:", test_generic_for())

-- 5. Nested loops and conditions
local function test_complex(n)
    local result = 0
    for i = 1, n do
        if i % 2 == 0 then
            for j = 1, i do
                result = result + j
            end
        else
            result = result - i
        end
    end
    return result
end
print("Test Complex (5):", test_complex(5))

print("--- Test Finished ---")
