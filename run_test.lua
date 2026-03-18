local patch = require("patch")
local mem3 = patch.alloc(32)
patch.write_cstring(mem3, "abcdefghijklmnopqrstuvwxyz")
patch.memmove(patch.add_ptr(mem3, 2), mem3, 5) -- abcde -> cdefg => ababcdeghijklmnopqrstuvwxyz
local s2 = patch.read_cstring(mem3)
print(s2)
