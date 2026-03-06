local lisp_runner = require("lisp_glr")
local lisp_code = "(progn\
(setq x 10)\
(setq y 20)\
(setq z (* x y))\
(if (> z 150) (progn\
  (print \"z is large\" z)\
) (progn\
  (print \"z is small\" z)\
))\
)"
-- Execute the transpiled lisp code
lisp_runner.run(lisp_code)
