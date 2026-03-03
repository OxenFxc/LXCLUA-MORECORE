local f = io.open("test.lua", "w")
f:write("print('hello')")
f:close()
os.execute("./luac -o test.luac test.lua")

local str = require("string")
str.file2png("test.luac", "test_luac.png")
local loaded, err = loadfile("test_luac.png")
if loaded then
    print("loaded bytecode successfully!")
else
    print("load bytecode failed:", err)
end
