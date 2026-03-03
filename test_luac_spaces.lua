local f = io.open("test.lua", "w")
f:write("print('hello')")
f:close()
os.execute("./luac -o test.luac test.lua")

local f2 = io.open("test.luac", "a")
f2:write("     ")
f2:close()

local loaded, err = loadfile("test.luac")
if loaded then
    print("loaded successfully!")
else
    print("load failed:", err)
end
