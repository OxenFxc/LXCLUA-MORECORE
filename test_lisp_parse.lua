local lisp = require("lisp_glr")
local lexer = require("lexer")
local code = [[
(progn
(setq a 1)
(setq b 2)
(setq c (+ a b))
(if (== c 3) (progn
  (print "Obfuscation Success!")
) (progn
  (print "Fail")
))
)
]]
local tokens = lexer(code)
for i, t in ipairs(tokens) do print(i, t.type, t.value) end
local parses = lisp.parse(code)
print("Parsed Lisp")
