local lexer = require("lexer")
local tokens = lexer("print(\"Hello\", 1+2)")
for i, t in ipairs(tokens) do
    print(t.type, t.value)
end
