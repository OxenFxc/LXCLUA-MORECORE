local res = bool.toexpr(true, 32)
local f, err = load("return " .. res)
if not f then print(err) end
