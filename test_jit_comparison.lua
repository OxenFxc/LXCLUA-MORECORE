
local function test_eq(a, b) return a == b end
local function test_lt(a, b) return a < b end
local function test_le(a, b) return a <= b end

print("Testing Basic Comparisons...")
assert(test_eq(10, 10) == true)
assert(test_eq(10, 20) == false)
assert(test_lt(10, 20) == true)
assert(test_lt(20, 10) == false)
assert(test_le(10, 10) == true)
assert(test_le(10, 20) == true)
assert(test_le(20, 10) == false)
print("Basic Comparisons Passed")

local function test_eqi(a) return a == 100 end
local function test_lti(a) return a < 100 end
local function test_lei(a) return a <= 100 end

print("Testing Immediate Comparisons...")
assert(test_eqi(100) == true)
assert(test_eqi(200) == false)
assert(test_lti(50) == true)
assert(test_lti(150) == false)
assert(test_lei(100) == true)
assert(test_lei(50) == true)
assert(test_lei(150) == false)
print("Immediate Comparisons Passed")

print("Testing Float Comparisons...")
assert(test_eq(10.5, 10.5) == true)
assert(test_lt(10.5, 20.5) == true)
print("Float Comparisons Passed")

print("Testing String Comparisons...")
assert(test_eq("abc", "abc") == true)
assert(test_eq("abc", "def") == false)
assert(test_lt("abc", "def") == true)
print("String Comparisons Passed")

local function test_and(a, b) return a and b end
local function test_or(a, b) return a or b end

print("Testing Logical Operators...")
assert(test_and(true, true) == true)
assert(test_and(true, false) == false)
assert(test_or(false, true) == true)
assert(test_or(false, false) == false)
local res = test_or(10, 20)
assert(res == 10, "Expected 10, got " .. tostring(res))
res = test_and(10, 20)
assert(res == 20, "Expected 20, got " .. tostring(res))
print("Logical Operators Passed")

print("All JIT Comparison Tests Passed")
