local lua2lisp = require("lua2lisp_glr")

local lua_code = [[
local a = 1
local b = 2
local c = a + b
if c == 3 then
    print("Obfuscation Success!")
else
    print("Fail")
end
]]

print("Original Lua Code:")
print(lua_code)
print("---------------------------------")

local lisp_code = lua2lisp.convert(lua_code)
print("Converted Lisp Code:")
print(lisp_code)
print("---------------------------------")

print("Executing Lisp code via Lisp GLR Runner:")
local lisp_runner = require("lisp_glr")
lisp_runner.run(lisp_code)
