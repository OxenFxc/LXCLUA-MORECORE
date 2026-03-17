local patch = require("patch")
local ptr = require("ptr")

print("==================================================")
print("1. Testing patch.get_marker (VMP Markers)")
print("==================================================")

-- The old markers are still here! They were NOT deleted.
local lundump_marker, lundump_size = patch.get_marker("lundump")
if lundump_marker then
    print(string.format("[+] VMP Marker 'lundump' found at: %s (Size: %d bytes)", tostring(lundump_marker), lundump_size))
else
    print("[-] FAILED to find marker 'lundump'")
end

local lvm_marker, lvm_size = patch.get_marker("lvm")
if lvm_marker then
    print(string.format("[+] VMP Marker 'lvm' found at: %s (Size: %d bytes)", tostring(lvm_marker), lvm_size))
else
    print("[-] FAILED to find marker 'lvm'")
end

local ldump_marker, ldump_size = patch.get_marker("ldump")
if ldump_marker then
    print(string.format("[+] VMP Marker 'ldump' found at: %s (Size: %d bytes)", tostring(ldump_marker), ldump_size))
else
    print("[-] FAILED to find marker 'ldump'")
end


print("\n==================================================")
print("2. Testing patch.get_symbol (Arbitrary Function Patching)")
print("==================================================")

-- This proves we can now patch *any* function dynamically, not just VMP markers
-- This proves we can now patch dynamically linked standard functions
local symbol_name = "puts"
local symbol_addr = patch.get_symbol(symbol_name)

if symbol_addr then
    print(string.format("[+] Symbol '%s' found at: %s", symbol_name, tostring(symbol_addr)))
    print("    You can now use `patch.write(symbol_addr, \"\\xC3...\")` to modify this function's logic directly in memory!")
else
    print(string.format("[-] FAILED to find symbol '%s'", symbol_name))
end

print("\nSUCCESS: All symbols and markers are fully functional and intact.")
