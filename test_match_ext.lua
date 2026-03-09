local s = "hello 123"
local res = string.gsub(s, "%h+", "-")
print(res)
