local function fib(n)
   local a, b = 0, 1
   for i = 1, n do
     a, b = b, a + b
   end
   return a
 end

 local start = os.clock()
 local res = fib(100)
 local dt = os.clock() - start


 print(string.format("结果: %d", res))
 print(string.format("耗时: %.6f 秒", dt))
