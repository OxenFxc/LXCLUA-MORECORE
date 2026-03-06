local lua2lisp = require("lua2lisp_glr")

local input_file = arg[1]
local output_file = arg[2]

if not input_file or not output_file then
    print("Usage: ./lxclua obfuscate.lua <input.lua> <output.lua>")
    os.exit(1)
end

local f = io.open(input_file, "r")
local code = f:read("*a")
f:close()

local out_code = lua2lisp.obfuscate(code)

local f_out = io.open(output_file, "w")
f_out:write(out_code)
f_out:close()

print("Obfuscation complete: " .. output_file)
