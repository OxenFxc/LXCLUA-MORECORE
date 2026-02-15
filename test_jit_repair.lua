local function inner_addi(b)
  for i = 1, 1000 do
    b = b + 1
  end
  return b
end

local function test_addi()
  local b = inner_addi(0)
  assert(b == 1000)
end

local function inner_band(a, b)
  local c = 0
  for i = 1, 1000 do
    c = a & b
  end
  return c
end

local function test_band()
  local a = 0x0F
  local b = 0xF0
  local c = inner_band(a, b)
  assert(c == 0)
end

local function test_table_safety_inner(t, k, v)
  for i = 1, 1000 do
    t[k] = v -- OP_SETTABLE
    t[i] = i -- OP_SETI
  end
end

local function test_table_safety()
  local t = {}
  local k = "key"
  local v = "value"
  test_table_safety_inner(t, k, v)
end

print("Testing ADDI...")
test_addi()
print("Testing BAND...")
test_band()
print("Testing Table Safety...")
test_table_safety()
print("All tests passed!")
