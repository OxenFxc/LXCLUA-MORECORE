local res = thread.createx(function()
    print("Inside thread")
    return 7
end)
print("Result:", res)
assert(res == 7, "Expected 7, got " .. tostring(res))
print("Test passed!")
