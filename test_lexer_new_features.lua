local code = [[
local a = {
    test = 1
}
while a.test < 10 do
    a.test = a.test + 1
end
for i = 1, 10 do
    print(i)
end
repeat
    a.test = a.test - 1
until a.test == 0
a:b(nil, true, false, #a, not true, -1)
]]
local lexer = require("lexer")
local obfuscated = lexer.obfuscate(code, 0, 0, 0)
print(obfuscated)
