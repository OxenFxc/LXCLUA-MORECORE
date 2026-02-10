
namespace ADD {
  function add(a, b)
    return a + b
  end
  val = 888
}

print("ADD object:", ADD)
local func = ADD::add
print("ADD::add type:", type(func))
print("ADD::add value:", func)

local res = func(10, 20)
print("func(10, 20) result:", res)

local res2 = ADD::add(10, 20)
print("ADD::add(10, 20) result:", res2)

if res2 ~= 30 then
  error("Expected 30")
end

print("ADD::val =", ADD::val)
if ADD::val ~= 888 then
  error("Expected 888")
end

function test_label()
  ::continue::
  return 0
end
print("test_label() =", test_label())
if test_label() ~= 0 then
  error("Label ambiguity check failed")
end

namespace INNER {
  function get_outer_val()
    return ADD::val
  end
  x = 100
  function get_local_x()
    return x
  end
}

if INNER::get_outer_val() ~= 888 then
  error("Internal access failed")
end

if INNER::get_local_x() ~= 100 then
  error("Local var inside namespace failed")
end

print("All tests passed!")
