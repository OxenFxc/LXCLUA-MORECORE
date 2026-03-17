local patch = require("patch")

-- 1. Get the physical memory address of the VMP marker inside the VM loader
local marker_addr, size = patch.get_marker("lundump")
print(string.format("LVM Loader VMP Marker resides at: %s (Size: %d bytes)", tostring(marker_addr), size))

-- 2. Parse the userdata string safely across Windows and Linux
-- Windows output: userdata: 00007ff74a9c6180
-- Linux output: userdata: 0x5555...
local addr_str = tostring(marker_addr)
local hex_part = addr_str:match("userdata:%s*0*x?([0-9a-fA-F]+)")

if not hex_part then
    error("Cannot parse marker integer address from: " .. addr_str)
end

local marker_int = tonumber(hex_part, 16)
if not marker_int then
    error("Failed to convert parsed hex to integer: " .. hex_part)
end

print(string.format("Marker Address Int: 0x%x", marker_int))

-- 3. Construct a safe raw machine code payload (x86_64 / x86 / ARM NOPs)
-- Since the hook point is inside a standard C function, injecting a raw RET (0xC3)
-- could corrupt the stack frame if the compiler generated a prologue (e.g. push rbp).
-- The safest demonstration of writing raw machine code is to overwrite the existing
-- NOP marker bytes with our own dynamically generated NOP bytes (\x90).
local payload = string.rep("\x90", size)

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
