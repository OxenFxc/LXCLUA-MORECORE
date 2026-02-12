
-- Test script for prevent_recompile feature

print("Testing string.dump prevent_recompile...")

local function secret_func()
  return "this is secret"
end

-- Default behavior: prevent_recompile = true
local dumped = string.dump(secret_func)
print("Dumped size:", #dumped)

-- Load the dumped chunk
local loaded_func = load(dumped)
if not loaded_func then
  print("Failed to load dumped chunk!")
  os.exit(1)
end

print("Loaded function output:", loaded_func())

-- Dump the loaded function again
local dumped_again = string.dump(loaded_func)
print("Dumped again size:", #dumped_again)

if dumped == dumped_again then
  print("SUCCESS: Dumped again content matches original (prevent_recompile worked).")
else
  print("FAILURE: Dumped again content differs!")
  print("Dumped header:", string.byte(dumped, 1, 10))
  print("Again header: ", string.byte(dumped_again, 1, 10))
  os.exit(1)
end

-- Test disabling prevention
print("\nTesting prevent_recompile = false...")
local dumped_disabled = string.dump(secret_func, {prevent_recompile = false})
local loaded_disabled = load(dumped_disabled)
local dumped_disabled_again = string.dump(loaded_disabled)

-- Normally, timestamps change, so binary chunks differ
if dumped_disabled ~= dumped_disabled_again then
  print("SUCCESS: Normal dump differs (timestamp change expected).")
else
  print("WARNING: Normal dump matches? Check if timestamps are working.")
end

-- Test mixing
print("\nTesting mixing...")
-- Load a prevented chunk, then try to dump with prevent_recompile=false?
-- If we return original chunk, we ignore options.
local dumped_override = string.dump(loaded_func, {prevent_recompile = false})
if dumped_override == dumped then
  print("INFO: Dumping a protected function returns original chunk even if requested otherwise (as expected by 'return original content').")
else
  print("INFO: Dumping a protected function with prevent=false generated new chunk.")
end

print("All tests passed.")
