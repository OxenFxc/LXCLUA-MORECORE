local arr = {10, 20}
local function print_all(a, b, c)
  print(a, b, c)
end
print_all(1, ...arr)
print_all(1, ...arr, 3)
