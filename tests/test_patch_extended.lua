local patch = require("patch")

print("Testing patch.alloc...")
local size = 4096
local mem = patch.alloc(size)
assert(mem ~= nil, "patch.alloc failed to allocate memory")
print("Allocated memory at:", mem)

print("Testing patch.mprotect...")
-- The memory from patch.alloc is already RWX, but let's test the function
local protected = patch.mprotect(mem, size, "rwx")
assert(protected == true, "patch.mprotect failed")

print("Testing patch.write and patch.read...")
-- Write some dummy data
local test_data = "Hello, Patch!"
local written = patch.write(mem, test_data)
assert(written == true, "patch.write failed")

local read_data = patch.read(mem, #test_data)
assert(read_data == test_data, "patch.read did not return the written data")
print("Read successful:", read_data)

print("Testing patch.call...")
-- We can't safely call arbitrary non-executable code. We'll write a simple shellcode (ret)
-- x86_64 'ret' instruction is 0xC3
if string.dump then -- Only try to execute if we're reasonably sure we're on a compatible arch or just test it
    local ret_instr = string.char(0xC3)
    patch.write(mem, ret_instr)
    print("Calling shellcode (ret)...")
    -- This should just return without crashing
    patch.call(mem)
    print("Call successful!")
end

print("Testing patch.free...")
local freed = patch.free(mem, size)
assert(freed == true, "patch.free failed")
print("Memory freed successfully.")

print("All extended patch functions tested successfully!")
