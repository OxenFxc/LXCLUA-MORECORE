local patch = require("patch")

print("==================================================")
print("[*] Clean Shellcode Execution: Calculating 1 + 1")
print("==================================================")

local mem_size = 64
local shellcode_addr = patch.alloc(mem_size)

if not shellcode_addr then
    print("[-] Failed to allocate memory.")
    os.exit(1)
end

print(string.format("[+] Allocated %d bytes of memory at: %s", mem_size, tostring(shellcode_addr)))

-- 最简单的 x86_64 汇编：
-- mov eax, 1    -> B8 01 00 00 00
-- add eax, 1    -> 83 C0 01
-- ret           -> C3
local raw_shellcode = "\xB8\x01\x00\x00\x00\x83\xC0\x01\xC3"

-- 写入机器码并修改权限 (W^X 合规)
patch.write(shellcode_addr, raw_shellcode)
patch.mprotect(shellcode_addr, mem_size, "rx")
print("[*] Shellcode written and protected as RX.")

print("[*] Executing injected shellcode using new patch.call_ret()...")

-- 使用我们新加的 C 接口，它会自动把 eax/rax 寄存器的返回值抓取出来
local success, result = pcall(function()
    return patch.call_ret(shellcode_addr)
end)

if success then
    print(string.format("[+] Shellcode executed safely! Direct return value: %d", result))
else
    print("[-] Shellcode execution failed: " .. tostring(result))
end

patch.free(shellcode_addr, mem_size)
print("[+] Cleanup complete.")
