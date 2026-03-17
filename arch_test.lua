local patch = require("patch")
local arch = patch.get_arch()
local opcodes = patch.get_opcodes()
for k, v in pairs(opcodes.arm) do
  print(k, #v)
end
