local patch = require("patch")

print("==================================================")
print("[*] Phase 1: Engine SMC (Self-Modifying Code) Patch")
print("==================================================")

-- 1. 动态解析 C 引擎中真实暴露的 Marker 地址
-- patch 库内部硬编码支持了 "lvm", "lundump", "ldump"
local marker_name = "lvm"
local hook_addr, hook_size = patch.get_marker(marker_name)

if hook_addr then
    print(string.format("[+] Found VMP marker '%s' at address: %s (Size: %d bytes)", marker_name, tostring(hook_addr), hook_size))

    -- 2. 解除目标内存的写保护 (修改为 RWX)
    -- 注意: patch.mprotect 的第三个参数是字符串标志位 "rwx"
    print("[*] Modifying memory protection to RWX...")
    local mprotect_success = patch.mprotect(hook_addr, hook_size, "rwx")

    if mprotect_success then
        print("[+] Memory protection changed to RWX successfully.")

        -- 3. 读取原始内存 (备份)
        local original_bytes = patch.read(hook_addr, hook_size)
        if original_bytes then
            print(string.format("[*] Original bytes (first 4): %02X %02X %02X %02X",
                string.byte(original_bytes, 1), string.byte(original_bytes, 2),
                string.byte(original_bytes, 3), string.byte(original_bytes, 4)))
        end

        -- 4. 覆写底层内存
        -- 这里我们只写入几个 NOP 指令作为演示 (\x90)，不破坏整个虚拟机流程
        -- 实际对抗中，这里会替换为混淆过的跳转指令或新的检查逻辑
        local nop_sled = "\x90\x90\x90\x90"
        print("[*] Injecting NOP sled into VMP marker...")
        patch.write(hook_addr, nop_sled)

        -- 5. 核心安全提示：严格遵守 W^X 原则，立即撤销 WRITE 权限，防止进程被操作系统查杀
        print("[*] Restoring memory protection to RX (W^X compliance)...")
        patch.mprotect(hook_addr, hook_size, "rx")

        print("[+] SMC Patch successfully applied! Original logic modified.")
    else
        print("[-] Failed to change memory protection. SMC aborted.")
    end
else
    print("[-] Failed to find VMP marker. SMC aborted.")
end

print("\n==================================================")
print("[*] Phase 2: Raw Shellcode Allocation and Execution")
print("==================================================")

-- 1. 分配一块 1024 字节的系统内存
-- patch.alloc 默认会分配 PROT_READ | PROT_WRITE | PROT_EXEC 内存 (RWX)
local mem_size = 1024
local shellcode_addr = patch.alloc(mem_size)
print(string.format("[+] Allocated %d bytes of memory at: %s", mem_size, tostring(shellcode_addr)))

if shellcode_addr then
    -- 2. 准备一段独立的机器码 (x86_64/x86 架构兼容)
    -- 这里最安全的测试代码是简单的 RET (\xC3)，它直接返回调用者，不会引起寄存器状态混乱
    local raw_shellcode = "\xC3"

    -- 3. 将机器码写入刚刚分配的内存中
    patch.write(shellcode_addr, raw_shellcode)
    print("[*] Shellcode (RET) written successfully.")

    -- 4. 设置内存权限为可读可执行 (遵循 W^X 原则)
    patch.mprotect(shellcode_addr, mem_size, "rx")
    print("[*] Memory protected as RX.")

    -- 5. 直接把这块内存当作 C 函数调用，引擎底层指针跳转并执行
    print("[*] Executing injected shellcode...")

    -- 注意：这里的 patch.call 必须保证机器码以正常的调用约定返回 (例如通过 ret)，否则会崩溃
    local success, err = pcall(function()
        patch.call(shellcode_addr)
    end)

    if success then
        print("[+] Shellcode executed successfully and returned safely!")
    else
        print("[-] Shellcode execution failed: " .. tostring(err))
    end

    -- 6. 抹除痕迹，释放内存
    patch.free(shellcode_addr, mem_size)
    print("[+] Memory freed. Cleanup complete.")
else
    print("[-] Failed to allocate memory.")
end
