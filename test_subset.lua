local GLR = require("glr")
local lexer = require("lexer")
local lua2lisp = require("lua2lisp_glr")

local lua_code = "local a = 1"
local lisp = lua2lisp.convert(lua_code)
print(lisp)
