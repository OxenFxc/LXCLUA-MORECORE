local function fib(n)
  local a, b = 0, 1
  for i = 1, n do
    a, b = b, a + b
  end
  return a
end

local start = os.clock()
local res = fib(1000000)
local dt = os.clock() - start

print("fib(10000) =", res)
print(string.format("耗时: %.6f 秒", dt))
