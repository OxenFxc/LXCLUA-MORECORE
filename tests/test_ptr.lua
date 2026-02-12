-- Test script for Real Pointer System

print("Testing Pointer System...")

-- 1. Check if ptr library exists
if not ptr then
  print("Error: ptr library not loaded")
  os.exit(1)
end

-- 2. Allocate memory
local size = 10
local p = ptr.malloc(size)
print("Allocated pointer:", p)

-- 3. Check type
local t = type(p)
print("Type of pointer:", t)
if t ~= "pointer" then
  print("Error: expected type 'pointer', got " .. t)
  os.exit(1)
end

-- 4. Arithmetic (Addition)
local p2 = p + 1
local diff = p2 - p
print("p + 1 diff:", diff)
if diff ~= 1 then
  print("Error: p + 1 arithmetic failed")
  os.exit(1)
end

-- 5. Arithmetic (Subtraction)
local p3 = p + 5
local diff2 = p3 - p2
print("p + 5 - (p + 1) diff:", diff2)
if diff2 ~= 4 then
  print("Error: subtraction failed")
  os.exit(1)
end

-- 6. Indexing (Read/Write)
p[0] = 65 -- 'A'
p[1] = 66 -- 'B'
p[2] = 255 -- Max byte

print("p[0]:", p[0])
print("p[1]:", p[1])
print("p[2]:", p[2])

if p[0] ~= 65 then print("Error: p[0] write/read failed") os.exit(1) end
if p[1] ~= 66 then print("Error: p[1] write/read failed") os.exit(1) end
if p[2] ~= 255 then print("Error: p[2] write/read failed") os.exit(1) end

-- 7. Typed read/write (Legacy compatibility)
ptr.write(p, "int", 123456)
local val = ptr.read(p, "int")
print("ptr.read int:", val)
if val ~= 123456 then print("Error: ptr.read int failed") os.exit(1) end

-- 8. Equality
local p_alias = p
if p ~= p_alias then print("Error: equality check failed") os.exit(1) end
if p == p2 then print("Error: equality check (different) failed") os.exit(1) end

-- 9. Free
ptr.free(p)
print("Freed pointer")

print("All pointer tests passed!")
