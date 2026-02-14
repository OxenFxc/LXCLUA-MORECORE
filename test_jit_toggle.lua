
print("Testing JIT Toggle Feature")

if not jit then
  print("Error: 'jit' library not loaded")
  os.exit(1)
end

-- Check initial status (should be true/on by default)
local status = jit.status()
print("Initial JIT status:", status)
if status ~= true then
  print("Error: Expected JIT to be enabled by default")
  os.exit(1)
end

-- Turn JIT off
print("Disabling JIT...")
jit.off()
status = jit.status()
print("JIT status after off:", status)
if status ~= false then
  print("Error: Expected JIT to be disabled")
  os.exit(1)
end

-- Define a function to test compilation behavior (heuristically)
-- Since we can't easily check if it's compiled from Lua without debug info,
-- we just ensure it runs without crashing.
local function test_func(n)
  local sum = 0
  for i = 1, n do
    sum = sum + i
  end
  return sum
end

print("Running test_func with JIT OFF...")
local res = test_func(1000)
print("Result:", res)
if res ~= 500500 then
  print("Error: Calculation failed")
  os.exit(1)
end

-- Turn JIT on
print("Enabling JIT...")
jit.on()
status = jit.status()
print("JIT status after on:", status)
if status ~= true then
  print("Error: Expected JIT to be enabled")
  os.exit(1)
end

print("Running test_func with JIT ON...")
-- Run enough times to trigger compilation or just run
for i = 1, 10 do
  res = test_func(1000)
end
print("Result:", res)
if res ~= 500500 then
  print("Error: Calculation failed")
  os.exit(1)
end

print("JIT Toggle Test Passed")
