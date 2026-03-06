local lisp = require("lisp_glr")
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
lisp.run(code)
