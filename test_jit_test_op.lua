print("--- JIT OP_TEST and OP_TESTSET Test Start ---")

local function test_op_test(v, k)
    if v then
        return "true"
    else
        return "false"
    end
end

local function test_op_testset_or(v)
    -- This generates OP_TESTSET with k=1
    local x = v or "default"
    return x
end

local function test_op_testset_and(v)
    -- This generates OP_TESTSET with k=0
    local x = v and "value"
    return x
end

-- Warmup JIT
for i = 1, 1000 do
    test_op_test(true, 0)
    test_op_test(false, 0)
    test_op_testset_or(nil)
    test_op_testset_or("value")
    test_op_testset_and(true)
    test_op_testset_and(false)
end

-- Verify Results
assert(test_op_test(true, 0) == "true")
assert(test_op_test(false, 0) == "false")
assert(test_op_test(nil, 0) == "false")
assert(test_op_test(1, 0) == "true")

assert(test_op_testset_or(nil) == "default")
assert(test_op_testset_or(false) == "default")
assert(test_op_testset_or("foo") == "foo")
assert(test_op_testset_or(true) == true)

assert(test_op_testset_and(true) == "value")
assert(test_op_testset_and("foo") == "value")
assert(test_op_testset_and(false) == false)
assert(test_op_testset_and(nil) == nil)

print("--- JIT OP_TEST and OP_TESTSET Test Passed ---")
