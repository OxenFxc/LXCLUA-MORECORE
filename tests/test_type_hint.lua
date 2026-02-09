local function test_type_hint()
  print("Testing Type Hints...")

  -- 1. Variable type hint
  local var: string = "hello world"
  assert(type(var) == "string")

  -- 2. Function type hint
  local function myfunc(a: string, b: string): number
    return tonumber(a) + tonumber(b)
  end
  assert(myfunc("1", "2") == 3)

  -- 3. Mismatch warning (should be visible in stderr)
  print("Expect warning below:")
  local res = myfunc(1, "1")

  -- 4. Multiple return values
  local function get_status(): (bool, string)
    return true, "OK!"
  end
  local ok, msg = get_status()
  assert(ok == true)
  assert(msg == "OK!")

  -- 5. Union Types
  local u: string|int = "s"
  u = 1

  -- 6. Table Types
  local point: { x: number, y: number } = { x = 1, y = 2 }
  assert(point.x == 1)

  -- 7. Named Types
  $type Point = { x: number, y: number }
  local p: Point = { x = 1, y = 2 }
  assert(p.x == 1)

  print("Type Hints OK")
end

local function test_destructuring()
  print("Testing Destructuring...")

  -- 1. Array destructuring
  local t = { 3, 6, 9 }
  local [a, b, c] = t
  assert(a == 3)
  assert(b == 6)
  assert(c == 9)

  -- 2. Table destructuring
  local t2 = { name = "John", age = 42 }
  local { name, age } = t2
  assert(name == "John")
  assert(age == 42)

  print("Destructuring OK")
end

test_type_hint()
test_destructuring()
