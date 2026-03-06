local lua2lisp = require("lua2lisp_glr")

local lua_code = [[
local x = 10
local y = 20
local z = x * y
if z > 150 then
    print("z is large", z)
else
    print("z is small", z)
end
]]

local lisp = lua2lisp.convert(lua_code)
print(lisp)
require("lisp_glr").run(lisp)
