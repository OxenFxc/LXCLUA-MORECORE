local int = 10
local float = 20.5
local double = 30.5
local char = "c"
local void = nil
local long = 40
local bool = true

print("Soft keywords test:")
print("int =", int)
print("float =", float)
print("double =", double)
print("char =", char)
print("long =", long)
print("bool =", bool)

-- Test function parameter
local function test(int, float)
  return int + float
end
print("Function param test:", test(1, 2))

-- Test table field
local t = { int = 100, float = 200 }
print("Table field test:", t.int, t.float)

-- Test function name
local function int()
  return "function int"
end
print("Function name test:", int())

if int() == "function int" and t.int == 100 then
    print("Soft keywords: PASS")
else
    print("Soft keywords: FAIL")
end
