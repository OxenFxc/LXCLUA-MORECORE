local patch = require("patch")
local ptr = require("ptr")

-- 1. Create a dummy Lua function with an endless loop
local function infinite_loop()
    print("WARNING: This should never execute!")
end

-- 2. Get the physical memory address of the VMP marker inside the VM loader
local marker_addr, size = patch.get_marker("lundump")
print(string.format("LVM Loader VMP Marker resides at: %s (Size: %d bytes)", tostring(marker_addr), size))

-- To calculate the relative jump offset, we need the integer value of the marker address
-- We use FFI or `tostring(marker_addr)` trick to parse the address if `ptr.toint` is not available.
local addr_str = tostring(marker_addr)
local marker_int = tonumber(addr_str:match("0x[0-9a-fA-F]+"))
if not marker_int then
    print("Cannot parse marker integer address:", addr_str)
    -- Fallback for testing: write a RET instruction (0xC3) to immediately return
    -- The C function calling VMP_MARKER is `void lundump_vmp_hook_point(void)`.
    -- If we overwrite it with `RET`, it safely returns and bypasses the rest of the NOPs.
    print("Applying simple RET payload...")
    patch.write(marker_addr, string.char(0xC3) .. string.rep("\144", size - 1))
    print("Successfully injected raw Machine Code!")
    return
end

print(string.format("Marker Address Int: 0x%x", marker_int))

-- 3. Let's create an x86_64 raw assembly payload that calls an endless loop or returns.
-- The simplest payload to prove we can execute raw machine code without crashing the VM:
-- The hook is called from `luaU_undump` as `lundump_vmp_hook_point()`.
-- It expects a `RET` (0xC3) to return back to `luaU_undump`.
-- We will write a payload that modifies RAX (even if it's discarded) and returns.
-- Payload:
-- mov eax, 0x1337  (\xB8\x37\x13\x00\x00)
-- ret              (\xC3)

local payload = "\xB8\x37\x13\x00\x00\xC3"
local padding = string.rep("\x90", size - #payload)
payload = payload .. padding

print("\n>>> INJECTING RAW MACHINE CODE INTO VM EXECUTABLE SEGMENT (lundump.c) <<<\n")
patch.write(marker_addr, payload)

print("Constructed Raw Machine Code Payload:")
local hex_payload = ""
for i = 1, #payload do
    hex_payload = hex_payload .. string.format("%02X ", string.byte(payload, i))
end
print(hex_payload)

-- 4. Trigger the Loader (lundump.c) by loading a dummy bytecode
print("\n>>> Triggering luaU_undump via loadfile() to execute the injected Machine Code... <<<\n")
local f = io.open("temp_dummy.bc", "w")
f:write(string.dump(function() print("Hello from dummy") end))
f:close()

local chunk = loadfile("temp_dummy.bc")
print("VM Loader executed successfully! The raw machine code payload ran without issues.")
os.remove("temp_dummy.bc")
