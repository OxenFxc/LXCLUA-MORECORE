local code = [[
local a = {
    test = 1,
    foo = "bar",
    [1] = 2
}
for k, v in pairs(a) do
    print(k, v)
end
]]
local lexer = require("lexer")
local obfuscated = lexer.obfuscate(code, 0, 0, 0)
print(obfuscated)
