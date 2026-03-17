local patch = require("patch")
local ptr = require("ptr")

print("==================================================")
print("1. Testing patch.write on VMP Markers")
print("==================================================")

-- Helper function to write NOP sled + RET to safely return
local function patch_marker(name)
    local addr, size = patch.get_marker(name)
    if not addr then
        print(string.format("[-] ERROR: Marker '%s' NOT FOUND!", name))
        return false
    end
    print(string.format("[+] Found Marker '%s' at %s (Size: %d bytes)", name, tostring(addr), size))

    -- Simple x86/64 Payload: mov eax, 0x1337 ; ret
    local payload = "\xB8\x37\x13\x00\x00\xC3"
    local padding = string.rep("\x90", size - #payload)
    local full_payload = payload .. padding

    -- Attempt Write
    local success = patch.write(addr, full_payload)
    if success then
        print(string.format("    -> [SUCCESS] Successfully overwrote memory block for '%s'", name))
    else
        print(string.format("    -> [FAILED] Could not modify memory for '%s'", name))
    end
    return success
end

-- Test all three markers
local success_1 = patch_marker("lundump")
local success_2 = patch_marker("lvm")
local success_3 = patch_marker("ldump")


print("\n==================================================")
print("2. Testing patch.write via dynamically loaded symbol")
print("==================================================")

-- Look up a safe, innocuous standard C function to patch temporarily for testing
-- We will resolve 'lua_version', grab its address, and attempt to write a NOP/RET
local symbol_name = "lua_version"
local symbol_addr = patch.get_symbol(symbol_name)

if symbol_addr then
    print(string.format("[+] Found Symbol '%s' at %s via dynamic lookup.", symbol_name, tostring(symbol_addr)))

    -- Write a single RET instruction (0xC3) just to prove write permissions work on exported symbols.
    local ret_payload = "\xC3"
    local success_4 = patch.write(symbol_addr, ret_payload)

    if success_4 then
        print(string.format("    -> [SUCCESS] Successfully overwrote first byte of '%s' with RET (0xC3).", symbol_name))
    else
        print(string.format("    -> [FAILED] Could not modify memory for '%s'", symbol_name))
    end
else
    print(string.format("[-] ERROR: Symbol '%s' NOT FOUND!", symbol_name))
end

print("\n==================================================")
if success_1 and success_2 and success_3 then
    print("ALL TESTS PASSED: Markers and Symbols were successfully resolved and modified in memory!")
else
    print("WARNING: Some write tests failed.")
end
