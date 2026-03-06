local lexer = require("lexer")
local tokens = lexer([[
local a = 1
local b = 2
local c = a + b
if c == 3 then
    print("Obfuscation Success!")
else
    print("Fail")
end
]])
for i, t in ipairs(tokens) do
    print(t.type, t.value)
end
