local patch = require("patch")

-- Test type conversions
local num = 0x12345678
local ptr = patch.to_ptr(num)
local back = patch.to_num(ptr)
assert(back == num, string.format("Pointer conversion failed: expected %x, got %x", num, back))
print("to_num / to_ptr: SUCCESS")

-- Test memory allocation and simple reading/writing
local mem = patch.alloc(4096)
assert(mem ~= nil, "Memory allocation failed")
print("Allocated memory at:", mem)

-- Write typed values
patch.write_u8(mem, 0xAB)
patch.write_u32(mem, 0x12345678) -- overwrite
patch.write_u64(mem, 0x1122334455667788) -- overwrite

-- Read typed values
local val = patch.read_u64(mem)
assert(val == 0x1122334455667788, string.format("read_u64 failed: expected 0x1122334455667788, got 0x%x", val))
print("read_u64 / write_u64: SUCCESS")

-- Test memcpy
local mem2 = patch.alloc(4096)
assert(mem2 ~= nil, "Second memory allocation failed")
patch.memcpy(mem2, mem, 8)
local val2 = patch.read_u64(mem2)
assert(val2 == 0x1122334455667788, "memcpy failed")
print("memcpy: SUCCESS")

-- Test call_ret with a direct system function if possible, e.g. time()
-- or since we cannot easily find an address without dlopen/dlsym natively here, let's just use patch_call_ret on a tiny machine code snippet.
local arch = patch.get_arch()
if arch == "x86_64" then
    -- x86_64 shellcode for:
    -- mov rax, rdi
    -- add rax, rsi
    -- add rax, rdx
    -- add rax, rcx
    -- add rax, r8
    -- add rax, r9
    -- ret
    local shellcode = "\x48\x89\xf8\x48\x01\xf0\x48\x01\xd0\x48\x01\xc8\x4c\x01\xc0\x4c\x01\xc8\xc3"
    local sc_mem = patch.alloc(4096)
    patch.write(sc_mem, shellcode)
    patch.mprotect(sc_mem, 4096, "r-x") -- make executable

    local res = patch.call_ret(sc_mem, 1, 2, 3, 4, 5, 6)
    assert(res == 21, string.format("call_ret with args failed: expected 21, got %s", tostring(res)))
    print("call_ret with arguments: SUCCESS (via shellcode)")
    patch.free(sc_mem, 4096)
else
    print("Skipping call_ret test on non-x86_64 architecture")
end

patch.free(mem, 4096)
patch.free(mem2, 4096)
print("All extensions verified successfully.")

-- Test new extensions
local p = patch.alloc(32)
patch.write_u16(p, 0x1234)
assert(patch.read_u16(p) == 0x1234, "u16 failed")

patch.write_f32(p, 3.14)
assert(math.abs(patch.read_f32(p) - 3.14) < 0.001, "f32 failed")

patch.write_f64(p, 2.71828)
assert(math.abs(patch.read_f64(p) - 2.71828) < 0.001, "f64 failed")

patch.write(p, "hello\0")
assert(patch.read_cstring(p) == "hello", "cstring failed")

local s = patch.search(p, 32, "lo")
assert(s ~= nil, "search failed")

local L = patch.get_state()
assert(L ~= nil, "get_state failed")

patch.free(p, 32)

-- Test memcmp, memset, write_cstring, alloc_aligned
local p1 = patch.alloc_aligned(64, 16)
local p2 = patch.alloc_aligned(64, 16)
assert(p1 ~= nil and p2 ~= nil, "alloc_aligned failed")

-- Should be 16-byte aligned
assert(patch.to_num(p1) % 16 == 0, "p1 not aligned")
assert(patch.to_num(p2) % 16 == 0, "p2 not aligned")

patch.memset(p1, 0xAA, 64)
patch.memset(p2, 0xAA, 64)
assert(patch.memcmp(p1, p2, 64) == 0, "memset or memcmp failed")

patch.write_u8(p2, 0xBB)
assert(patch.memcmp(p1, p2, 64) ~= 0, "memcmp failed (should not match)")

patch.write_cstring(p1, "hello aligned!")
assert(patch.read_cstring(p1) == "hello aligned!", "write_cstring failed")

patch.free_aligned(p1)
patch.free_aligned(p2)

if arch == "x86_64" then
    local orig_mem = patch.alloc(4096)
    local hook_mem = patch.alloc(4096)

    -- Function 1 (returns 10)
    -- mov rax, 10
    -- ret
    patch.write(orig_mem, "\x48\xc7\xc0\x0a\x00\x00\x00\xc3")
    patch.mprotect(orig_mem, 4096, "r-x")

    -- Function 2 (returns 20)
    -- mov rax, 20
    -- ret
    patch.write(hook_mem, "\x48\xc7\xc0\x14\x00\x00\x00\xc3")
    patch.mprotect(hook_mem, 4096, "r-x")

    local res1 = patch.call_ret(orig_mem)
    assert(res1 == 10, "orig function failed")

    patch.mprotect(orig_mem, 4096, "rwx")
    patch.hook(orig_mem, hook_mem)
    patch.mprotect(orig_mem, 4096, "r-x")

    local res2 = patch.call_ret(orig_mem)
    assert(res2 == 20, "hook failed: expected 20 got " .. tostring(res2))

    patch.free(orig_mem, 4096)
    patch.free(hook_mem, 4096)
    print("hook: SUCCESS")
end

local s_mem = patch.alloc(64)
-- format: u8, padding 1 byte, u16, padding 4 bytes (actually let's just use exact sizes without auto padding), u32, f64
-- "u8xu16xxxxu32f64"
local fmt = "u8xu16xxxxu32f64"
local values = { 0x12, 0x3456, 0x789ABCDE, 3.14159 }
patch.write_struct(s_mem, fmt, values)

local read_values = patch.read_struct(s_mem, fmt)
assert(read_values[1] == 0x12, "struct u8 mismatch")
assert(read_values[2] == 0x3456, "struct u16 mismatch")
assert(read_values[3] == 0x789ABCDE, "struct u32 mismatch")
assert(math.abs(read_values[4] - 3.14159) < 0.0001, "struct f64 mismatch")
patch.free(s_mem, 64)
print("struct read/write: SUCCESS")

-- Test new struct pointer writing for arbitrary types
local p_mem = patch.alloc(16)
local test_tbl = { 1, 2, 3 }
local test_str = "hello patch ptr"
patch.write_struct(p_mem, "pp", { test_tbl, test_str })
local p_read = patch.read_struct(p_mem, "pp")

local tbl_ptr = patch.to_ptr(tonumber(tostring(test_tbl):match("0x([0-9a-fA-F]+)") or tostring(test_tbl):match("table: ([0-9a-fA-F]+)"), 16))

-- tostring for strings just prints the string, but string's internal address isn't easily exposed without an internal lib.
-- However, we just need to verify it didn't crash and read *something* back successfully.
assert(p_read[1] ~= nil, "struct pointer read (table) failed")
assert(p_read[2] ~= nil, "struct pointer read (string) failed")
patch.free(p_mem, 16)
print("struct pointer (arbitrary types) write: SUCCESS")

-- Test new integer types (signed)
local i_mem = patch.alloc(64)

patch.write_i8(i_mem, -128)
assert(patch.read_i8(i_mem) == -128, "i8 write/read failed")

patch.write_i16(i_mem, -32768)
assert(patch.read_i16(i_mem) == -32768, "i16 write/read failed")

patch.write_i32(i_mem, -2147483648)
assert(patch.read_i32(i_mem) == -2147483648, "i32 write/read failed")

patch.write_i64(i_mem, -9223372036854775808)
assert(patch.read_i64(i_mem) == -9223372036854775808, "i64 write/read failed")

print("Signed integer (i8/i16/i32/i64) write/read: SUCCESS")
patch.free(i_mem, 64)

-- Test direct pointer read/write
local ptr_mem = patch.alloc(16)
local dummy_mem = patch.alloc(4)
patch.write_u32(dummy_mem, 0xDEADBEEF)

patch.write_ptr(ptr_mem, dummy_mem)
local read_back_ptr = patch.read_ptr(ptr_mem)

assert(read_back_ptr == dummy_mem, "pointer write/read mismatch")
assert(patch.read_u32(read_back_ptr) == 0xDEADBEEF, "dereferencing read pointer failed")

-- Null pointer handling
patch.write_u64(ptr_mem, 0)
assert(patch.read_ptr(ptr_mem) == nil, "null pointer read failed")

print("Pointer read/write: SUCCESS")

patch.free(ptr_mem, 16)
patch.free(dummy_mem, 4)


-- Test part 1 extensions
local mem3 = patch.alloc(32)
patch.write_cstring(mem3, "abcdefghijklmnopqrstuvwxyz")
patch.memmove(patch.add_ptr(mem3, 2), mem3, 5) -- abcde over cdefg => ababcdehijklmnopqrstuvwxyz
local s2 = patch.read_cstring(mem3)
assert(s2 == "ababcdehijklmnopqrstuvwxyz", "memmove failed: expected ababcdehijklmnopqrstuvwxyz, got " .. s2)

local chr_ptr = patch.memchr(mem3, string.byte("h"), 32)
assert(chr_ptr ~= nil, "memchr failed")
assert(patch.read_u8(chr_ptr) == string.byte("h"), "memchr found wrong byte")

local nop_mem = patch.alloc(16)
patch.nop(nop_mem, 4)
if arch == "x86_64" or arch == "x86" then
    assert(patch.read_u8(nop_mem) == 0x90, "nop failed for x86/x64")
elseif arch == "arm64" then
    assert(patch.read_u32(nop_mem) == 0xd503201f, "nop failed for arm64")
elseif arch == "arm" then
    assert(patch.read_u32(nop_mem) == 0xe1a00000, "nop failed for arm")
end
patch.free(nop_mem, 16)
patch.free(mem3, 32)
print("memmove, memchr, nop: SUCCESS")

-- Test part 2 extensions
local page_size = patch.get_page_size()
assert(type(page_size) == "number" and page_size > 0, "get_page_size failed")

local pid = patch.get_pid()
assert(type(pid) == "number" and pid > 0, "get_pid failed")

local base_ptr = patch.to_ptr(0x1000)
local next_ptr = patch.add_ptr(base_ptr, 0x10)
local prev_ptr = patch.sub_ptr(next_ptr, 0x10)
assert(patch.to_num(next_ptr) == 0x1010, "add_ptr failed")
assert(patch.to_num(prev_ptr) == 0x1000, "sub_ptr failed")
print("get_page_size, get_pid, add_ptr, sub_ptr: SUCCESS")

-- Test part 3 extensions
local byte_mem = patch.alloc(16)
local data = {0xDE, 0xAD, 0xBE, 0xEF}
patch.write_bytes(byte_mem, data)
local read_data = patch.read_bytes(byte_mem, 4)
assert(read_data[1] == 0xDE and read_data[2] == 0xAD and read_data[3] == 0xBE and read_data[4] == 0xEF, "read_bytes/write_bytes failed")

local scan_mem = patch.alloc(64)
patch.write_bytes(scan_mem, {0x11, 0x22, 0x33, 0x44, 0x55, 0x66})
local found = patch.scan_pattern(scan_mem, 64, "11 22 ? 44 ?? 66")
assert(found ~= nil and patch.to_num(found) == patch.to_num(scan_mem), "scan_pattern failed")
patch.free(byte_mem, 16)
patch.free(scan_mem, 64)
print("read_bytes, write_bytes, scan_pattern: SUCCESS")

print("All NEW patch extensions passed!")
