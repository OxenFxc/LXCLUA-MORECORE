-- Test __mindex
local t = setmetatable({}, {
    __mindex = function(self, key)
        if key == "foo" then return "bar" end
    end
})
assert(t.foo == "bar")
assert(t.baz == nil)
print("__mindex passed")

-- Test Safe Navigation
local t2 = nil
assert(t2?.foo == nil)
t2 = { foo = "bar" }
assert(t2?.foo == "bar")
print("Safe navigation passed")

-- Test 'in' operator
local t3 = { a = 1, b = 2 }
assert("a" in t3)
assert(not ("c" in t3))
print("'in' operator passed")

-- Test Enum
enum Color
    Red
    Green = 10
    Blue
end

assert(Color.Red == 0)
assert(Color.Green == 10)
assert(Color.Blue == 11)
print("Enum passed")

-- Test Export (in local scope)
do
    export local x = 42
    -- In local scope, export might just register it.
    -- To test export properly we might need modules, but let's see if it parses and runs.
    assert(x == 42)
end
print("Export (local) passed")

print("All Pluto features tests passed!")
