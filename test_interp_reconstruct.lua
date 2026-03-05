local lexer = require("lexer")
local code = [[
local a = "hello ${name}"
local b = _raw"path\to\file"
]]
local tokens = lexer.lex(code)
local out = lexer.to_code(tokens)
print(out)
local f = assert(load(out))
print("compiled successfully!")
