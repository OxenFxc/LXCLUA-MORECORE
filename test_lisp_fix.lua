local lisp = require("lisp_glr")
local code = [[
(if true
  (print "True branch")
  (print "False branch")
)
]]
lisp.run(code)
