local patch = require("patch")

print("==================================================")
print("[*] Raw Shellcode Execution: Calculating 1 + 1")
print("==================================================")

-- 1. 分配一块 128 字节的系统内存 (默认是 RWX: PROT_READ | PROT_WRITE | PROT_EXEC)
local mem_size = 128
local shellcode_addr = patch.alloc(mem_size)

if not shellcode_addr then
    print("[-] Failed to allocate memory.")
    os.exit(1)
end

print(string.format("[+] Allocated %d bytes of memory at: %s", mem_size, tostring(shellcode_addr)))

-- 2. 准备计算 1 + 1 的机器码 (x86_64 架构)
-- 在 x86_64 C 调用约定中，函数的返回值存储在 RAX (或 EAX) 寄存器中。
--
-- 对应的汇编指令:
-- mov eax, 1    -> B8 01 00 00 00  (将 1 放入 EAX)
-- add eax, 1    -> 83 C0 01        (将 1 加到 EAX)
-- ret           -> C3              (函数返回，返回值在 EAX)
--
local raw_shellcode = "\xB8\x01\x00\x00\x00" .. -- mov eax, 1
                      "\x83\xC0\x01"         .. -- add eax, 1
                      "\xC3"                    -- ret

-- 3. 将机器码写入刚刚分配的内存中
patch.write(shellcode_addr, raw_shellcode)
print("[*] Shellcode written successfully.")

-- 4. 设置内存权限为可读可执行 (遵循 W^X 原则)
patch.mprotect(shellcode_addr, mem_size, "rx")
print("[*] Memory protected as RX (W^X compliant).")

-- 5. 调用执行这段机器码
print("[*] Executing injected shellcode...")

-- pcall 捕获可能崩溃的情况，但如果是正常 return 的机器码就不会崩溃
-- 注意：引擎层面的 patch.call 如果没有捕获 RAX 返回值，这可能没法直接在 Lua 层面打印出结果。
-- 但如果你修改过 lpatchlib.c 让 patch_call 返回整数 (lua_pushinteger(L, func())), 就能看到 2。
-- 在原版 C 实现中 `patch.call` 直接返回了 0：
--   void (*func)(void) = (void (*)(void))address; func(); return 0;
-- 所以默认它是拿不到返回值的。我教你一种通过指针直接写回内存的方法来读取结果：

-- ========== 改进的 Shellcode：将结果写回我们提供的内存指针 ==========
-- 在 x86_64 调用约定下，如果函数有参数，第一个参数 (我们的指针) 存放在 RDI 中。
-- 如果 patch.call 不传参，RDI 的值是不确定的。
-- 由于原本的 patch.call(addr) 没有任何参数传入，我们干脆再分配一块 8 字节的内存当 "共享变量"。
local result_ptr = patch.alloc(8)
-- 将这个指针强行硬编码进机器码里：
-- mov rax, 1
-- add rax, 1
-- mov rcx, <result_ptr_address>  (注意：使用 rcx 这种 volatile 寄存器，不要用 rbx 这种 callee-saved 寄存器破坏 C 调用栈)
-- mov [rcx], rax
-- ret

-- 将 result_ptr (LightUserData) 转换为 64位无符号整数表示的地址字符串，以便写入汇编
-- Lua 的 userdata 打印出来类似: userdata: 0x55d04526b000 (Linux) 或 userdata: 00007FF... (Windows)
local addr_str = tostring(result_ptr):match("userdata:%s*0*x?([0-9a-fA-F]+)")
if not addr_str then
    print("[-] Failed to parse pointer address from userdata string: " .. tostring(result_ptr))
    os.exit(1)
end

-- 将十六进制字符串手动转化为 8字节 小端序 的二进制字符串
local function to_little_endian_8bytes(hex_str)
    -- 补齐 16 个字符 (8 字节)
    hex_str = string.rep("0", 16 - #hex_str) .. hex_str
    local bytes = {}
    for i = #hex_str, 1, -2 do
        table.insert(bytes, string.char(tonumber(hex_str:sub(i-1, i), 16)))
    end
    return table.concat(bytes)
end

local ptr_bytes = to_little_endian_8bytes(addr_str)

-- 重新构造高级机器码：
local advanced_shellcode =
    "\x48\xC7\xC0\x01\x00\x00\x00" .. -- mov rax, 1
    "\x48\x83\xC0\x01"             .. -- add rax, 1
    "\x48\xB9" .. ptr_bytes        .. -- mov rcx, <8_byte_address>
    "\x48\x89\x01"                 .. -- mov [rcx], rax (写入结果到共享内存)
    "\xC3"                            -- ret

-- 重新赋予写权限
patch.mprotect(shellcode_addr, mem_size, "rwx")
patch.write(shellcode_addr, advanced_shellcode)
patch.mprotect(shellcode_addr, mem_size, "rx")

print(string.format("[*] Advanced Shellcode injected! Shared Memory Address: %s", tostring(result_ptr)))

local success, err = pcall(function()
    patch.call(shellcode_addr)
end)

if success then
    -- 从共享内存中读取这 8 个字节
    local result_bytes = patch.read(result_ptr, 8)

    -- 将小端序的 8 字节还原为 Lua 的数字
    local sum = 0
    for i = 1, 8 do
        sum = sum + string.byte(result_bytes, i) * (256 ^ (i - 1))
    end

    print("[+] Shellcode executed safely!")
    print(string.format("[+] >>> The result of 1 + 1 is: %d <<<", sum))
else
    print("[-] Shellcode execution failed: " .. tostring(err))
end

-- 清理内存
patch.free(shellcode_addr, mem_size)
patch.free(result_ptr, 8)
print("[+] Cleanup complete.")
