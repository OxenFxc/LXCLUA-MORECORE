print("Testing Control Flow Crash Isolation...")

-- 1. Switch Statement
print("Testing Switch...")
local val = 1
local output = ""
switch (val) do
    case 1:
        output = "One"
        break
    case "test":
        output = "Test"
        break
    default:
        output = "Other"
end
if output ~= "One" then error("Switch failed") end
print("Switch Passed")

-- 2. Defer
print("Testing Defer...")
local deferred = false
do
    defer do deferred = true end
end
if not deferred then error("Defer failed") end
print("Defer Passed")

-- 3. Try-Catch
print("Testing Try-Catch...")
local caught = false
try
    error("Something went wrong")
catch(e)
    caught = true
finally
    -- cleanup
end
if not caught then error("Try-Catch failed") end
print("Try-Catch Passed")
